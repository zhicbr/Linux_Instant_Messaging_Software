#ifndef READWRITELOCK_H
#define READWRITELOCK_H

#include <QObject>
#include <pthread.h>

class ReadWriteLock : public QObject {
    Q_OBJECT
public:
    explicit ReadWriteLock(QObject *parent = nullptr);
    ~ReadWriteLock();

    // 初始化读写锁
    bool init();
    
    // 销毁读写锁
    bool destroy();
    
    // 获取读锁
    bool lockForRead();
    
    // 获取写锁
    bool lockForWrite();
    
    // 尝试获取读锁
    bool tryLockForRead();
    
    // 尝试获取写锁
    bool tryLockForWrite();
    
    // 释放锁
    bool unlock();
    
    // 检查读写锁是否有效
    bool isValid() const { return m_initialized; }

private:
    pthread_rwlock_t m_rwlock;
    bool m_initialized;
};

// 读锁守卫类
class ReadLocker {
public:
    explicit ReadLocker(ReadWriteLock *lock) : m_lock(lock) {
        if (m_lock) {
            m_locked = m_lock->lockForRead();
        } else {
            m_locked = false;
        }
    }
    
    ~ReadLocker() {
        unlock();
    }
    
    bool isLocked() const { return m_locked; }
    
    void unlock() {
        if (m_locked && m_lock) {
            m_lock->unlock();
            m_locked = false;
        }
    }

private:
    ReadWriteLock *m_lock;
    bool m_locked;
};

// 写锁守卫类
class WriteLocker {
public:
    explicit WriteLocker(ReadWriteLock *lock) : m_lock(lock) {
        if (m_lock) {
            m_locked = m_lock->lockForWrite();
        } else {
            m_locked = false;
        }
    }
    
    ~WriteLocker() {
        unlock();
    }
    
    bool isLocked() const { return m_locked; }
    
    void unlock() {
        if (m_locked && m_lock) {
            m_lock->unlock();
            m_locked = false;
        }
    }

private:
    ReadWriteLock *m_lock;
    bool m_locked;
};

#endif // READWRITELOCK_H
