#ifndef _BLOB_H_
#define _BLOB_H_

class Blob {

public:
    Blob() { }

    Blob(double x, double y, double area, int frameNumber, long time) :
        x(x),
        y(y),
        area(area),
        frameNum(frameNumber),
        time(time)
    {

    }

    Blob(double x, double y, int frameNumber, long time) :
        x(x),
        y(y),
        frameNum(frameNumber),
        time(time)
    {

    }

    void setClusterId(int id) {
        clusterId = id;
    }

    int getClusterId() {
        return clusterId;
    }

    ~Blob() {

    }

    double x;
    double y;
    double area;
    int frameNum;
    long time;
private:
    int clusterId;
};

#endif

