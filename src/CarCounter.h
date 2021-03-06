#ifndef _CAR_COUNTER_H_
#define _CAR_COUNTER_H_

#include "Blob.h"
#include "EastboundObjectIdentifier.h"
#include "WestboundObjectIdentifier.h"
#include "opencv2/core/core.hpp"
#include <vector>
#include <list>

class CarCounter {

public:
    CarCounter();

    void setBlobLogFile(const char * path) {
        blobLogFilePath = path;
    }

    void setObjectsDetectedLogFile(const char * path) {
        objectDetectedFilePath = path;
    }

    ~CarCounter();

    int getCarCount() {
        return eastboundCarCount + westboundCarCount;
    }

    double getAvgSpeed(int numFrames);

    int classifyObjects(bool forceAll, long currentTime);

    int processBlobs(std::vector<Blob*>& blobs, long currentTime, bool retryUnclassified=false);

private:
    int findBestFit(Blob& b, std::list<ObjectIdentifier*> objects, ObjectIdentifier** bestFit);

    double distanceThreshold;
    double expectedPathSlope;
    int eastboundCarCount;
    int westboundCarCount;
    int bikeCount;
    int streetcarCount;
    int xBoundary;
    bool useSlopeOfPathFilter;

    cv::Rect * boundaries;

    std::list<ObjectIdentifier*> eastboundObjects;
    std::list<ObjectIdentifier*> westboundObjects;
    std::vector<Blob*> unidentifiedBlobs;

    int rosCreated;

    int frameNumber;

    // Logging
    int writeToBlobLog(const char * line);
    int writeToObjectsLog(const char * line);
    // Also deletes blobs after logging them
    //void blobsToLogAndRemove(int numBlobs);
    void logBlob(Blob& b);
    void logIdentifiedObject(ObjectIdentifier& oi, long time);
    FILE * blobLogFile;
    FILE * objectsLogFile;
    const char * blobLogFilePath;
    const char * objectDetectedFilePath;
    int writesToBlobLog;
    int writesToObjectLog;

    const static int MIN_NUM_POINTS = 7;
    const static int MIN_FRAME_TIMEOUT = 5 * 10; // 10 seconds at 5 fps
    const static int MAX_FRAME_TIMEOUT = 5 * 60 * 2; // 2 minutes at 5 fps

    const static int NUM_POINTS_FOR_LINEAR_REGRESSION = 5;
    const static int NUM_POINTS_FOR_OVERLAP_CHECK = 2;
    const static int NUM_POINTS_TO_AVG = 5;
    const static double OVERLAP_PCT_THRESHOLD = 1; // 100%
    const static double NUM_PIXELS_LINEAR_REGRESSION_THRESHOLD = 5;
    const static double BOUNDS_THRESHOLD_PCT = 0.1;
    const static int VEHICLE_SIZE_THRESHOLD = 700;
    const static double SLOPE_FILTER_THRESHOLD = 0.1;
};

#endif
