#pragma once

#include <QVector>

class CircularBuffer {
public:
    explicit CircularBuffer(int capacity = 65536);

    void setCapacity(int capacity);
    int capacity() const;
    int size() const;
    bool isEmpty() const;
    void clear();

    void append(const QVector<double>& values);
    void append(const double* data, int count);
    QVector<double> latest(int count) const;

private:
    int m_capacity = 65536;
    int m_size = 0;
    int m_head = 0;
    QVector<double> m_data;
};
