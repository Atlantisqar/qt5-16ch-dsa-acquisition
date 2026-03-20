#include "core/circularbuffer.h"

#include <algorithm>
#include <cstring>

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

    if (count >= m_capacity) {
        const double* tail = data + (count - m_capacity);
        std::memcpy(m_data.data(), tail, static_cast<size_t>(m_capacity) * sizeof(double));
        m_head = 0;
        m_size = m_capacity;
        return;
    }

    const int firstChunk = std::min(count, m_capacity - m_head);
    std::memcpy(m_data.data() + m_head, data, static_cast<size_t>(firstChunk) * sizeof(double));

    const int remaining = count - firstChunk;
    if (remaining > 0) {
        std::memcpy(m_data.data(), data + firstChunk, static_cast<size_t>(remaining) * sizeof(double));
    }

    m_head = (m_head + count) % m_capacity;
    m_size = std::min(m_size + count, m_capacity);
}

QVector<double> CircularBuffer::latest(int count) const {
    if (count <= 0 || m_size <= 0) {
        return {};
    }
    const int realCount = std::min(count, m_size);
    QVector<double> out(realCount);

    const int start = (m_head - realCount + m_capacity) % m_capacity;
    const int firstChunk = std::min(realCount, m_capacity - start);
    std::memcpy(out.data(), m_data.constData() + start, static_cast<size_t>(firstChunk) * sizeof(double));

    const int remaining = realCount - firstChunk;
    if (remaining > 0) {
        std::memcpy(out.data() + firstChunk, m_data.constData(), static_cast<size_t>(remaining) * sizeof(double));
    }
    return out;
}
