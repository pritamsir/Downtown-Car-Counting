#include "ObjectIdentifier.h"

#include <limits>
#include <cmath>

using namespace std;
using namespace cv;

ObjectIdentifier::ObjectIdentifier(Blob* blob) :
    frameCount(0),
    id(2 + (globalID++ % 6)),
    closestDistToOrigin(999999), // TODO: maxint
    furthestDistToOrigin(0),
    closestBlob(blob),
    furthestBlob(blob),
    lastBlob(blob),
    numBlobs(0),
    currentTime(blob->time),
    startTime(blob->time),
    xyFilter(*(new KalmanFilter(4,2,0))),
    txFilter(*(new KalmanFilter(4,2,0))),
    tyFilter(*(new KalmanFilter(4,2,0))),
    speedFilter(*(new KalmanFilter(4,2,0))),
    type(UNKNOWN)
{
    Blob& b = *blob;
    xyFilter.statePre.at<float>(0) = b.x;
    xyFilter.statePre.at<float>(1) = b.y;
    xyFilter.statePre.at<float>(2) = 0;
    xyFilter.statePre.at<float>(3) = 0;
    xyFilter.transitionMatrix = *(Mat_<float>(4, 4) << 1,0,0,0,   0,1,0,0,  0,0,1,0,  0,0,0,1);
#if 1
    txFilter.statePre.at<float>(0) = b.time;
    txFilter.statePre.at<float>(1) = b.x;
    txFilter.statePre.at<float>(2) = 0;
    txFilter.statePre.at<float>(3) = 0;
    txFilter.transitionMatrix = *(Mat_<float>(4, 4) << 1,0,0,0,   0,1,0,0,  0,0,1,0,  0,0,0,1);

    tyFilter.statePre.at<float>(0) = b.time;
    tyFilter.statePre.at<float>(1) = b.y;
    tyFilter.statePre.at<float>(2) = 0;
    tyFilter.statePre.at<float>(3) = 0;
    tyFilter.transitionMatrix = *(Mat_<float>(4, 4) << 1,0,0,0,   0,1,0,0,  0,0,1,0,  0,0,0,1);
#endif
    setIdentity(xyFilter.measurementMatrix);
    //setIdentity(xyFilter.processNoiseCov, Scalar::all(1e-4));
    //setIdentity(xyFilter.measurementNoiseCov, Scalar::all(1e-1));
    //setIdentity(xyFilter.errorCovPost, Scalar::all(.1));
#if 1
    setIdentity(txFilter.measurementMatrix);
    //setIdentity(txFilter.processNoiseCov, Scalar::all(1e-4));
    //setIdentity(txFilter.measurementNoiseCov, Scalar::all(1e-1));
    //setIdentity(txFilter.errorCovPost, Scalar::all(.1));

    setIdentity(tyFilter.measurementMatrix);
    //setIdentity(tyFilter.processNoiseCov, Scalar::all(1e-4));
    //setIdentity(tyFilter.measurementNoiseCov, Scalar::all(1e-1));
    //setIdentity(tyFilter.errorCovPost, Scalar::all(.1));
    setIdentity(speedFilter.measurementMatrix);
#endif
    addBlob(b);
}

ObjectIdentifier::~ObjectIdentifier()
{
    printf("~ObjectIdentifier\n");
    for (int i = 0; i < blobs.size(); i++) {
        delete blobs.at(i);
    }
    delete &xyFilter;
    delete &txFilter;
    delete &tyFilter;
}

void ObjectIdentifier::updateTime(long inTime)
{
    currentTime = inTime;
}

long ObjectIdentifier::lastSeen()
{
    printf("Last Seen %ld current %ld blob %ld\n", currentTime - blobs.at(blobs.size() - 1)->time, currentTime, blobs.at(blobs.size() - 1)->time);
    return (currentTime - blobs.at(blobs.size() - 1)->time);
}

int ObjectIdentifier::getNumBlobs()
{
    return numBlobs;
}

int ObjectIdentifier::getFirstTime()
{
    return (blobs.at(0)->time);
}

long ObjectIdentifier::getLifetime()
{
    return (currentTime - startTime);
}

double ObjectIdentifier::getSpeed()
{
    double speed = (getLifetime() > 0 ? getDistanceTravelled() / getLifetime() : 0);
    return speed;
}

bool ObjectIdentifier::addBlob(Blob& b)
{
    numBlobs++;
    lastBlob = &b;
    blobs.push_back(&b);
    currentTime = b.time;

    // Keep track of closest and furthest blobs from origin TODO: why? maybe not needed anymore
    double distanceToOrigin = distance(b.x, b.y, 0, 0);

    if (distanceToOrigin > furthestDistToOrigin) {
        furthestBlob = &b;
        furthestDistToOrigin = distanceToOrigin;
    }

    if (distanceToOrigin < closestDistToOrigin) {
        closestBlob = &b;
        closestDistToOrigin = distanceToOrigin;
    }

    xyFilter.predict();
    txFilter.predict();
    tyFilter.predict();
    cv::Mat_<float> measurement(2,1);
    measurement(0) = b.time;
    measurement(1) = b.x;
    txFilter.correct(measurement);

    measurement(0) = b.time;
    measurement(1) = b.y;
    tyFilter.correct(measurement);

    measurement(0) = b.x;
    measurement(1) = b.y;
    xyFilter.correct(measurement);
}

int ObjectIdentifier::getFit(Blob& b)
{
    int score = 0;

    double distToLast = distanceFromLastBlob(b);
    double toPredictedXY = distToPredictedXY(b.x, b.y);
    double toPredictedTX = distToPredictedTX(b.time, b.x);
    double toPredictedTY = distToPredictedTY(b.time, b.y);
    double e1 = errXY(b.x, b.y);
    double e2 = errTX(b.time, b.x);
    double e3 = errTY(b.time, b.y);
    double e = sqrt(e1*e1 + e2*e2 + e3*e3);

    printf("ID %d XY %f TX %f TY %f   D %f      SumSq: %f\n", getId(), e1, e2, e3, distToLast, e);
    printf("Predicted XY %f TX %f TY %f\n", toPredictedXY, toPredictedTX, toPredictedTY);

    double actualRate = distToLast / ((double)b.time - (double)lastBlob->time) * 1000;
    printf("Speed %f  Size/lastBlob %f\n", actualRate, (double)sqrt(b.area) / (double)sqrt(lastBlob->area));
    if (getNumBlobs() <= 5) {
        score = (30 - distToLast) * 3;
#if 0
    } else if (e < 20 && e < minError) {
        minError = e;
        bestFit = oi;
        printf("Best fit accepted\n");
#endif
    } else {
        // These 3 contribute 200 / 300 possible score
        int xyScore = 100 - (toPredictedXY < 100 ? toPredictedXY : 100);
        int txScore = 100 - (toPredictedTX < 100 ? toPredictedTX : 100);
        int tyScore = 100 - (toPredictedTY < 100 ? toPredictedTY : 100);
        int xytScore = xyScore + txScore + tyScore - 100;
        if (xytScore < 0) xytScore = 0;
        // Contributes up to 120
        int linearityScore = (e <= 60) ? (60 - e) * 2 : 0;
        score = xytScore + linearityScore;
        if (linearityScore && distToLast < 20) {
            score += (20 - distToLast) * 5;
        }
    }
    printf("Score %d\n", score);
    return score;
}

void ObjectIdentifier::printPoints()
{
    for (int i = 0; i < blobs.size(); i++) {
        Blob& b = *blobs.at(i);
        printf("%ld,%f,%f,%d,%d\n", b.time, b.x, b.y, (int)b.area, id);
    }
}

double ObjectIdentifier::getDistanceTravelled()
{
    return distanceBetweenBlobs(*furthestBlob, *closestBlob);
}

double ObjectIdentifier::getXYSlope()
{
    pair<double,double> line = xyLeastSqrRegression(blobs, blobs.size());
    double slope = line.first;
    return slope;
}

double ObjectIdentifier::errXY(double x, double y)
{
    pair<double,double> line = xyLeastSqrRegression(blobs, max(10, (int)blobs.size()/2));
    double slope = line.first;
    double y_int = line.second;

    // Standard form
    double A = slope;
    double B = -1;
    double C = y_int;

    double distance = abs(A*x + B*y + C) / sqrt(A*A + B*B);
    return distance;
}

double ObjectIdentifier::errTX(long time, double x)
{
    pair<double,double> line = txLeastSqrRegression(blobs, max(10, (int)blobs.size()/2));
    double slope = line.first;
    double y_int = line.second;

    // Standard form
    double A = slope;
    double B = -1;
    double C = y_int;

    double distance = abs(A*time + B*x + C) / sqrt(A*A + B*B);
    return distance;
}

double ObjectIdentifier::errTY(long time, double y)
{
    pair<double,double> line = tyLeastSqrRegression(blobs, max(10, (int)blobs.size()/2));
    double slope = line.first;
    double y_int = line.second;

    // Standard form
    double A = slope;
    double B = -1;
    double C = y_int;

    double distance = abs(A*time + B*y + C) / sqrt(A*A + B*B);
    return distance;
}

double ObjectIdentifier::distFromExpectedY(double x, double y)
{
    if (blobs.size()  < 2) {
        return std::numeric_limits<double>::max(); // MAX_INT
    }

    pair<double,double> line = xyLeastSqrRegression(blobs, blobs.size());
    double slope = line.first;
    double y_int = line.second;
    double y_exp = slope * x + y_int;
    return abs(y - y_exp);
}

double ObjectIdentifier::distFromExpectedY(double y, long time)
{
    pair<double,double> line = tyLeastSqrRegression(blobs, blobs.size());
    double slope = line.first;
    double y_int = line.second;
    double y_exp = slope * time + y_int;
    return abs(y - y_exp);
}

double ObjectIdentifier::distFromExpectedX(double x, long time)
{
    pair<double,double> line = txLeastSqrRegression(blobs, blobs.size());
    double slope = line.first;
    double x_int = line.second;
    double x_exp = slope * time + x_int;
    return abs(x - x_exp);
}

double ObjectIdentifier::distToPredictedXY(double x, double y)
{
    Mat prediction = xyFilter.predict();
    //printf("Predicted X %f Y %f\n", prediction.at<float>(0), prediction.at<float>(1));
    return distance(x, y, prediction.at<float>(0), prediction.at<float>(1));
}

double ObjectIdentifier::distToPredictedTX(long time, double x)
{
    Mat prediction = txFilter.predict();
    //printf("Predicted T %f X %f\n", prediction.at<float>(0), prediction.at<float>(1));
    return distance(time, x, prediction.at<float>(0), prediction.at<float>(1));
}

double ObjectIdentifier::distToPredictedTY(long time, double y)
{
    Mat prediction = tyFilter.predict();
    //printf("Predicted T %f Y %f\n", prediction.at<float>(0), prediction.at<float>(1));
    return distance(time, y, prediction.at<float>(0), prediction.at<float>(1));
}

Blob& ObjectIdentifier::getLastBlob()
{
    return *lastBlob;
}

int ObjectIdentifier::getId()
{
    return id;
}

double ObjectIdentifier::distanceBetweenBlobs(Blob& b1, Blob& b2)
{
    return distance(b1.x, b1.y, b2.x, b2.y);
}

#if 1
vector<Blob*>& ObjectIdentifier::getBlobs()
{
    return blobs;
}
#endif

// Distance between 2 points
double ObjectIdentifier::distance(double x1, double y1, double x2, double y2)
{
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// TODO: store this calculation
double ObjectIdentifier::getAverageSize()
{
    int sizeSum = 0;
    for (int i = 0; i < blobs.size(); i++) {
        sizeSum += blobs.at(i)->area;
    }
    return (sizeSum / blobs.size());
}

bool ObjectIdentifier::inEndZone()
{
    double x = furthestBlob->x;
    double y = furthestBlob->y;
    return (x > 500 && y > 360);
}

double ObjectIdentifier::expectedY(double x)
{
    // TODO: CONTST for these and override for EastboundObjectDetector
    double slope = 0.185;
    double y_int = 270;
    double y_exp = slope * x + y_int;
    return y_exp;
}

double ObjectIdentifier::distanceFromLastBlob(Blob& b)
{
    return distanceBetweenBlobs(*lastBlob, b);
}



// HELPER FUNCTION

pair<double,double> ObjectIdentifier::txLeastSqrRegression(vector<Blob*>& blobs, int numPointsToUse)
{
   double SUMx = 0;     //sum of x values
   double SUMy = 0;     //sum of y values
   double SUMxy = 0;    //sum of x * y
   double SUMxx = 0;    //sum of x^2
   double SUMres = 0;   //sum of squared residue
   double res = 0;      //residue squared
   double slope = 0;    //slope of regression line
   double y_intercept = 0; //y intercept of regression line
   double SUM_Yres = 0; //sum of squared of the discrepancies
   double AVGy = 0;     //mean of y
   double AVGx = 0;     //mean of x
   double Yres = 0;     //squared of the discrepancies
   double Rsqr = 0;     //coefficient of determination

   numPointsToUse = (numPointsToUse <= blobs.size()) ? numPointsToUse : blobs.size();

   //calculate various sums
   for (int i = 0; i < numPointsToUse; i++)
   {
      //sum of x
      SUMx = SUMx + blobs.at(blobs.size() - 1 - i)->time;
      //sum of y
      SUMy = SUMy + blobs.at(blobs.size() - 1 - i)->x;
      //sum of squared x*y
      SUMxy = SUMxy + blobs.at(blobs.size() - 1 - i)->time * blobs.at(blobs.size() - 1 - i)->x;
      //sum of squared x
      SUMxx = SUMxx + blobs.at(blobs.size() - 1 - i)->time * blobs.at(blobs.size() - 1 - i)->time;
   }

   //calculate the means of x and y
   AVGy = SUMy / numPointsToUse;
   AVGx = SUMx / numPointsToUse;

   //slope or a1
   slope = (numPointsToUse * SUMxy - SUMx * SUMy) / (numPointsToUse * SUMxx - SUMx*SUMx);

   //y itercept or a0
   y_intercept = AVGy - slope * AVGx;
#if 0
   printf("x mean(AVGx) = %0.5E\n", AVGx);
   printf("y mean(AVGy) = %0.5E\n", AVGy);

   printf ("\n");
   printf ("The linear equation that best fits the given data:\n");
   printf ("       y = %2.8lfx + %2.8f\n", slope, y_intercept);
   printf ("------------------------------------------------------------\n");
   printf ("   Original (x,y)   (y_i - y_avg)^2     (y_i - a_o - a_1*x_i)^2\n");
   printf ("------------------------------------------------------------\n");
#endif
   //calculate squared residues, their sum etc.
   for (int i = 0; i < numPointsToUse; i++)
   {
      //current (y_i - a0 - a1 * x_i)^2
      Yres = pow((blobs.at(blobs.size() - 1 - i)->x - y_intercept - (slope * blobs.at(blobs.size() - 1 - i)->time)), 2);

      //sum of (y_i - a0 - a1 * x_i)^2
      SUM_Yres += Yres;

      //current residue squared (y_i - AVGy)^2
      res = pow(blobs.at(blobs.size() - 1 - i)->x - AVGy, 2);

      //sum of squared residues
      SUMres += res;
#if 0
      printf ("   (%0.2f %0.2f)      %0.5E         %0.5E\n",
       blobs.at(blobs.size() - 1 - i)->currentTime, blobs.at(blobs.size() - 1 - i)->x, res, Yres);
#endif
   }

   //calculate r^2 coefficient of determination
   Rsqr = (SUMres - SUM_Yres) / SUMres;
#if 0
   printf("--------------------------------------------------\n");
   printf("Sum of (y_i - y_avg)^2 = %0.5E\t\n", SUMres);
   printf("Sum of (y_i - a_o - a_1*x_i)^2 = %0.5E\t\n", SUM_Yres);
   printf("Standard deviation(St) = %0.5E\n", sqrt(SUMres / (numPointsToUse - 1)));
   printf("Standard error of the estimate(Sr) = %0.5E\t\n", sqrt(SUM_Yres / (numPointsToUse-2)));
   printf("Coefficent of determination(r^2) = %0.5E\t\n", (SUMres - SUM_Yres)/SUMres);
   printf("Correlation coefficient(r) = %0.5E\t\n", sqrt(Rsqr));
#endif
   txR = sqrt(Rsqr);
   pair<double,double> result(slope,y_intercept);
   return result;
}

pair<double,double> ObjectIdentifier::tyLeastSqrRegression(vector<Blob*> &blobs, int numPointsToUse)
{
   double SUMx = 0;     //sum of x values
   double SUMy = 0;     //sum of y values
   double SUMxy = 0;    //sum of x * y
   double SUMxx = 0;    //sum of x^2
   double SUMres = 0;   //sum of squared residue
   double res = 0;      //residue squared
   double slope = 0;    //slope of regression line
   double y_intercept = 0; //y intercept of regression line
   double SUM_Yres = 0; //sum of squared of the discrepancies
   double AVGy = 0;     //mean of y
   double AVGx = 0;     //mean of x
   double Yres = 0;     //squared of the discrepancies
   double Rsqr = 0;     //coefficient of determination

   numPointsToUse = (numPointsToUse <= blobs.size()) ? numPointsToUse : blobs.size();

   //calculate various sums
   for (int i = 0; i < numPointsToUse; i++)
   {
      //sum of x
      SUMx = SUMx + blobs.at(blobs.size() - 1 - i)->time;
      //sum of y
      SUMy = SUMy + blobs.at(blobs.size() - 1 - i)->y;
      //sum of squared x*y
      SUMxy = SUMxy + blobs.at(blobs.size() - 1 - i)->time * blobs.at(blobs.size() - 1 - i)->y;
      //sum of squared x
      SUMxx = SUMxx + blobs.at(blobs.size() - 1 - i)->time * blobs.at(blobs.size() - 1 - i)->time;
   }

   //calculate the means of x and y
   AVGy = SUMy / numPointsToUse;
   AVGx = SUMx / numPointsToUse;

   //slope or a1
   slope = (numPointsToUse * SUMxy - SUMx * SUMy) / (numPointsToUse * SUMxx - SUMx*SUMx);

   //y itercept or a0
   y_intercept = AVGy - slope * AVGx;
#if 0
   printf("x mean(AVGx) = %0.5E\n", AVGx);
   printf("y mean(AVGy) = %0.5E\n", AVGy);

   printf ("\n");
   printf ("The linear equation that best fits the given data:\n");
   printf ("       y = %2.8lfx + %2.8f\n", slope, y_intercept);
   printf ("------------------------------------------------------------\n");
   printf ("   Original (x,y)   (y_i - y_avg)^2     (y_i - a_o - a_1*x_i)^2\n");
   printf ("------------------------------------------------------------\n");
#endif
   //calculate squared residues, their sum etc.
   for (int i = 0; i < numPointsToUse; i++)
   {
      //current (y_i - a0 - a1 * x_i)^2
      Yres = pow((blobs.at(blobs.size() - 1 - i)->y - y_intercept - (slope * blobs.at(blobs.size() - 1 - i)->time)), 2);

      //sum of (y_i - a0 - a1 * x_i)^2
      SUM_Yres += Yres;

      //current residue squared (y_i - AVGy)^2
      res = pow(blobs.at(blobs.size() - 1 - i)->y - AVGy, 2);

      //sum of squared residues
      SUMres += res;
#if 0
      printf ("   (%0.2f %0.2f)      %0.5E         %0.5E\n",
       blobs.at(blobs.size() - 1 - i)->currentTime, blobs.at(blobs.size() - 1 - i)->y, res, Yres);
#endif
   }

   //calculate r^2 coefficient of determination
   Rsqr = (SUMres - SUM_Yres) / SUMres;
#if 0
   printf("--------------------------------------------------\n");
   printf("Sum of (y_i - y_avg)^2 = %0.5E\t\n", SUMres);
   printf("Sum of (y_i - a_o - a_1*x_i)^2 = %0.5E\t\n", SUM_Yres);
   printf("Standard deviation(St) = %0.5E\n", sqrt(SUMres / (numPointsToUse - 1)));
   printf("Standard error of the estimate(Sr) = %0.5E\t\n", sqrt(SUM_Yres / (numPointsToUse-2)));
   printf("Coefficent of determination(r^2) = %0.5E\t\n", (SUMres - SUM_Yres)/SUMres);
   printf("Correlation coefficient(r) = %0.5E\t\n", sqrt(Rsqr));
#endif
   tyR = sqrt(Rsqr);
   pair<double,double> result(slope,y_intercept);
   return result;
}

pair<double,double> ObjectIdentifier::xyLeastSqrRegression(vector<Blob*> &blobs, int numPointsToUse)
{
   double SUMx = 0;     //sum of x values
   double SUMy = 0;     //sum of y values
   double SUMxy = 0;    //sum of x * y
   double SUMxx = 0;    //sum of x^2
   double SUMres = 0;   //sum of squared residue
   double res = 0;      //residue squared
   double slope = 0;    //slope of regression line
   double y_intercept = 0; //y intercept of regression line
   double SUM_Yres = 0; //sum of squared of the discrepancies
   double AVGy = 0;     //mean of y
   double AVGx = 0;     //mean of x
   double Yres = 0;     //squared of the discrepancies
   double Rsqr = 0;     //coefficient of determination

   numPointsToUse = (numPointsToUse <= blobs.size()) ? numPointsToUse : blobs.size();

   //calculate various sums
   for (int i = 0; i < numPointsToUse; i++)
   {
      Blob& blob = *blobs.at(blobs.size() - 1 - i);
      //sum of x
      SUMx = SUMx + blob.x;
      //sum of y
      SUMy = SUMy + blob.y;
      //sum of squared x*y
      SUMxy = SUMxy + blob.x * blob.y;
      //sum of squared x
      SUMxx = SUMxx + blob.x * blob.x;
   }

   //calculate the means of x and y
   AVGy = SUMy / numPointsToUse;
   AVGx = SUMx / numPointsToUse;

   //slope or a1
   slope = (numPointsToUse * SUMxy - SUMx * SUMy) / (numPointsToUse * SUMxx - SUMx*SUMx);

   //y itercept or a0
   y_intercept = AVGy - slope * AVGx;
#if 0
   printf("x mean(AVGx) = %0.5E\n", AVGx);
   printf("y mean(AVGy) = %0.5E\n", AVGy);

   printf ("\n");
   printf ("The linear equation that best fits the given data:\n");
   printf ("       y = %2.8lfx + %2.8f\n", slope, y_intercept);
   printf ("------------------------------------------------------------\n");
   printf ("   Original (x,y)   (y_i - y_avg)^2     (y_i - a_o - a_1*x_i)^2\n");
   printf ("------------------------------------------------------------\n");
#endif
   //calculate squared residues, their sum etc.
   for (int i = 0; i < numPointsToUse; i++)
   {
      //current (y_i - a0 - a1 * x_i)^2
      Yres = pow((blobs.at(blobs.size() - 1 - i)->y - y_intercept - (slope * blobs.at(blobs.size() - 1 - i)->x)), 2);

      //sum of (y_i - a0 - a1 * x_i)^2
      SUM_Yres += Yres;

      //current residue squared (y_i - AVGy)^2
      res = pow(blobs.at(blobs.size() - 1 - i)->y - AVGy, 2);

      //sum of squared residues
      SUMres += res;
#if 0
      printf ("   (%0.2f %0.2f)      %0.5E         %0.5E\n",
       blobs.at(blobs.size() - 1 - i)->x, blobs.at(blobs.size() - 1 - i)->y, res, Yres);
#endif
   }

   //calculate r^2 coefficient of determination
   Rsqr = (SUMres - SUM_Yres) / SUMres;
#if 0
   printf("--------------------------------------------------\n");
   printf("Sum of (y_i - y_avg)^2 = %0.5E\t\n", SUMres);
   printf("Sum of (y_i - a_o - a_1*x_i)^2 = %0.5E\t\n", SUM_Yres);
   printf("Standard deviation(St) = %0.5E\n", sqrt(SUMres / (numPointsToUse - 1)));
   printf("Standard error of the estimate(Sr) = %0.5E\t\n", sqrt(SUM_Yres / (numPointsToUse-2)));
   printf("Coefficent of determination(r^2) = %0.5E\t\n", (SUMres - SUM_Yres)/SUMres);
   printf("Correlation coefficient(r) = %0.5E\t\n", sqrt(Rsqr));
#endif
   xyR = sqrt(Rsqr);
   pair<double,double> result(slope,y_intercept);
   return result;
}
