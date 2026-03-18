#include "core/circularbuffer.h"

#include <algorithm>

CircularBuffer::CircularBuffer(int capacity) {
    setCapacity(capacity);
}

void CircularBuffer::setCapacity(int capacity) {
    m_capacity = std::max(1, capacity);
    m_data.resize(m_capacity);
    m_head = 0;
    m_size = 0;
}

int CircularBuffer::capacity() const {
    return m_capacity;
}

int CircularBuffer::size() const {
    return m_size;
}

bool CircularBuffer::isEmpty() const {
    return m_size == 0;
}

void CircularBuffer::clear() {
    m_head = 0;
    m_size = 0;
}

void CircularBuffer::append(const QVector<double>& values) {
    append(values.constData(), values.size());
}

void CircularBuffer::append(const double* data, int count) {
    if (data == nullptr || count <= 0 || m_capacity <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        m_data[m_head] = data[i];
        m_head = (m_head + 1) % m_capacity;
        if (m_size < m_capacity) {
            ++m_size;
        }
    }
}

QVector<double> CircularBuffer::latest(int count) const {
    if (count <= 0 || m_size <= 0) {
        return {};
    }
    const int realCount = std::min(count, m_size);
    QVector<double> out;
    out.reserve(realCount);

    const int start = (m_head - realCount + m_capacity) % m_capacity;
    for (int i = 0; i < realCount; ++i) {
        const int idx = (start + i) % m_capacity;
        out.push_back(m_data[idx]);
    }
    return out;
}
