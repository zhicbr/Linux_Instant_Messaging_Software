#include "readwritelock.h"
#include <QDebug>
#include <string.h>
#include <errno.h>

ReadWriteLock::ReadWriteLock(QObject *parent) : QObject(parent), m_initialized(false) {
}

ReadWriteLock::~ReadWriteLock() {
    destroy();
}

bool ReadWriteLock::init() {
    if (m_initialized) {
        destroy();
    }
    
    // 初始化读写锁
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    
    // 设置读写锁属性，优先写锁
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    
    int ret = pthread_rwlock_init(&m_rwlock, &attr);
    pthread_rwlockattr_destroy(&attr);
    
    if (ret != 0) {
        qDebug() << "Failed to initialize read-write lock:" << strerror(ret);
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool ReadWriteLock::destroy() {
    if (m_initialized) {
        int ret = pthread_rwlock_destroy(&m_rwlock);
        if (ret != 0) {
            qDebug() << "Failed to destroy read-write lock:" << strerror(ret);
            return false;
        }
        
        m_initialized = false;
    }
    
    return true;
}

bool ReadWriteLock::lockForRead() {
    if (!m_initialized) {
        qDebug() << "Read-write lock is not initialized";
        return false;
    }
    
    int ret = pthread_rwlock_rdlock(&m_rwlock);
    if (ret != 0) {
        qDebug() << "Failed to acquire read lock:" << strerror(ret);
        return false;
    }
    
    return true;
}

bool ReadWriteLock::lockForWrite() {
    if (!m_initialized) {
        qDebug() << "Read-write lock is not initialized";
        return false;
    }
    
    int ret = pthread_rwlock_wrlock(&m_rwlock);
    if (ret != 0) {
        qDebug() << "Failed to acquire write lock:" << strerror(ret);
        return false;
    }
    
    return true;
}

bool ReadWriteLock::tryLockForRead() {
    if (!m_initialized) {
        qDebug() << "Read-write lock is not initialized";
        return false;
    }
    
    int ret = pthread_rwlock_tryrdlock(&m_rwlock);
    if (ret != 0) {
        if (ret == EBUSY) {
            // 锁被占用，不视为错误
            return false;
        }
        
        qDebug() << "Failed to try read lock:" << strerror(ret);
        return false;
    }
    
    return true;
}

bool ReadWriteLock::tryLockForWrite() {
    if (!m_initialized) {
        qDebug() << "Read-write lock is not initialized";
        return false;
    }
    
    int ret = pthread_rwlock_trywrlock(&m_rwlock);
    if (ret != 0) {
        if (ret == EBUSY) {
            // 锁被占用，不视为错误
            return false;
        }
        
        qDebug() << "Failed to try write lock:" << strerror(ret);
        return false;
    }
    
    return true;
}

bool ReadWriteLock::unlock() {
    if (!m_initialized) {
        qDebug() << "Read-write lock is not initialized";
        return false;
    }
    
    int ret = pthread_rwlock_unlock(&m_rwlock);
    if (ret != 0) {
        qDebug() << "Failed to unlock read-write lock:" << strerror(ret);
        return false;
    }
    
    return true;
}
