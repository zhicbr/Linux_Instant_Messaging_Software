#include "sharedmemory.h"
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <string.h>
#include <errno.h>

SharedMemory::SharedMemory(QObject *parent) : QObject(parent), m_shmid(-1), m_memory(nullptr), m_size(0), m_key(0) {
}

SharedMemory::~SharedMemory() {
    detach();
}

bool SharedMemory::create(const QString &key, size_t size) {
    // 如果已经附加到共享内存，先分离
    if (isValid()) {
        detach();
    }

    // 生成唯一键
    // 确保文件存在，因为ftok需要一个存在的文件
    QString keyFile = "/tmp/chat_server_key";
    QFile file(keyFile);
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
    }

    m_key = ftok(keyFile.toLocal8Bit().constData(), 'R');
    if (m_key == -1) {
        qDebug() << "Failed to generate key:" << strerror(errno);
        return false;
    }

    // 创建共享内存
    m_shmid = shmget(m_key, size, IPC_CREAT | 0666);
    if (m_shmid == -1) {
        qDebug() << "Failed to create shared memory:" << strerror(errno);
        return false;
    }

    // 附加到共享内存
    m_memory = shmat(m_shmid, nullptr, 0);
    if (m_memory == (void*)-1) {
        qDebug() << "Failed to attach to shared memory:" << strerror(errno);
        m_memory = nullptr;
        return false;
    }

    m_size = size;
    qDebug() << "Created shared memory with key" << m_key << "and size" << size;
    return true;
}

bool SharedMemory::detach() {
    if (m_memory) {
        if (shmdt(m_memory) == -1) {
            qDebug() << "Failed to detach from shared memory:" << strerror(errno);
            return false;
        }
        m_memory = nullptr;
    }

    if (m_shmid != -1) {
        // 标记共享内存可以被删除
        struct shmid_ds shmid_ds;
        if (shmctl(m_shmid, IPC_RMID, &shmid_ds) == -1) {
            qDebug() << "Failed to mark shared memory for deletion:" << strerror(errno);
            return false;
        }
        m_shmid = -1;
    }

    m_size = 0;
    return true;
}

bool SharedMemory::write(const QByteArray &data) {
    if (!isValid()) {
        qDebug() << "Shared memory is not valid";
        return false;
    }

    if (data.size() > m_size) {
        qDebug() << "Data size" << data.size() << "exceeds shared memory size" << m_size;
        return false;
    }

    // 写入数据大小
    *((size_t*)m_memory) = data.size();

    // 写入数据
    memcpy((char*)m_memory + sizeof(size_t), data.constData(), data.size());

    return true;
}

QByteArray SharedMemory::read() {
    if (!isValid()) {
        qDebug() << "Shared memory is not valid";
        return QByteArray();
    }

    // 读取数据大小
    size_t dataSize = *((size_t*)m_memory);

    if (dataSize > m_size - sizeof(size_t)) {
        qDebug() << "Invalid data size in shared memory:" << dataSize;
        return QByteArray();
    }

    // 读取数据
    return QByteArray((char*)m_memory + sizeof(size_t), dataSize);
}
