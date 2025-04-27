#include "semaphore.h"
#include <QDebug>
#include <string.h>
#include <errno.h>
#include <time.h>

Semaphore::Semaphore(QObject *parent) : QObject(parent), m_sem(SEM_FAILED) {
}

Semaphore::~Semaphore() {
    close();
}

bool Semaphore::create(const QString &name, unsigned int value) {
    // 如果已经打开信号量，先关闭
    if (isValid()) {
        close();
    }
    
    // 确保名称以/开头
    m_name = name;
    if (!m_name.startsWith('/')) {
        m_name = "/" + m_name;
    }
    
    // 创建信号量
    m_sem = sem_open(m_name.toLocal8Bit().constData(), O_CREAT, 0666, value);
    if (m_sem == SEM_FAILED) {
        qDebug() << "Failed to create semaphore:" << strerror(errno);
        return false;
    }
    
    qDebug() << "Created semaphore with name" << m_name;
    return true;
}

bool Semaphore::close() {
    if (m_sem != SEM_FAILED) {
        if (sem_close(m_sem) == -1) {
            qDebug() << "Failed to close semaphore:" << strerror(errno);
            return false;
        }
        
        // 尝试删除命名信号量
        if (sem_unlink(m_name.toLocal8Bit().constData()) == -1) {
            // 如果是因为信号量不存在而失败，不视为错误
            if (errno != ENOENT) {
                qDebug() << "Failed to unlink semaphore:" << strerror(errno);
                return false;
            }
        }
        
        m_sem = SEM_FAILED;
    }
    
    return true;
}

bool Semaphore::wait(int timeout_ms) {
    if (!isValid()) {
        qDebug() << "Semaphore is not valid";
        return false;
    }
    
    if (timeout_ms < 0) {
        // 无限等待
        if (sem_wait(m_sem) == -1) {
            qDebug() << "Failed to wait for semaphore:" << strerror(errno);
            return false;
        }
    } else {
        // 有超时的等待
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
            qDebug() << "Failed to get current time:" << strerror(errno);
            return false;
        }
        
        // 计算超时时间
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        if (sem_timedwait(m_sem, &ts) == -1) {
            if (errno == ETIMEDOUT) {
                qDebug() << "Semaphore wait timed out";
            } else {
                qDebug() << "Failed to wait for semaphore:" << strerror(errno);
            }
            return false;
        }
    }
    
    return true;
}

bool Semaphore::post() {
    if (!isValid()) {
        qDebug() << "Semaphore is not valid";
        return false;
    }
    
    if (sem_post(m_sem) == -1) {
        qDebug() << "Failed to post semaphore:" << strerror(errno);
        return false;
    }
    
    return true;
}

int Semaphore::getValue() {
    if (!isValid()) {
        qDebug() << "Semaphore is not valid";
        return -1;
    }
    
    int value;
    if (sem_getvalue(m_sem, &value) == -1) {
        qDebug() << "Failed to get semaphore value:" << strerror(errno);
        return -1;
    }
    
    return value;
}
