#include "filelock.h"
#include <QDebug>
#include <string.h>
#include <errno.h>

FileLock::FileLock(QObject *parent) : QObject(parent), m_fd(-1) {
}

FileLock::~FileLock() {
    close();
}

bool FileLock::open(const QString &filePath) {
    // 如果已经打开文件，先关闭
    if (isOpen()) {
        close();
    }
    
    // 打开文件
    m_fd = ::open(filePath.toLocal8Bit().constData(), O_RDWR | O_CREAT, 0666);
    if (m_fd == -1) {
        qDebug() << "Failed to open file:" << strerror(errno);
        return false;
    }
    
    m_filePath = filePath;
    qDebug() << "Opened file" << filePath;
    return true;
}

bool FileLock::close() {
    if (m_fd != -1) {
        // 确保解锁
        unlock();
        
        if (::close(m_fd) == -1) {
            qDebug() << "Failed to close file:" << strerror(errno);
            return false;
        }
        
        m_fd = -1;
        m_filePath.clear();
    }
    
    return true;
}

bool FileLock::lock(LockType type, bool wait) {
    if (!isOpen()) {
        qDebug() << "File is not open";
        return false;
    }
    
    struct flock fl;
    fl.l_type = (type == ReadLock) ? F_RDLCK : F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // 锁定整个文件
    
    int cmd = wait ? F_SETLKW : F_SETLK;
    
    if (fcntl(m_fd, cmd, &fl) == -1) {
        if (errno == EAGAIN || errno == EACCES) {
            qDebug() << "File is locked by another process";
        } else {
            qDebug() << "Failed to lock file:" << strerror(errno);
        }
        return false;
    }
    
    qDebug() << "Locked file" << m_filePath << "with" << (type == ReadLock ? "read" : "write") << "lock";
    return true;
}

bool FileLock::unlock() {
    if (!isOpen()) {
        qDebug() << "File is not open";
        return false;
    }
    
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // 解锁整个文件
    
    if (fcntl(m_fd, F_SETLK, &fl) == -1) {
        qDebug() << "Failed to unlock file:" << strerror(errno);
        return false;
    }
    
    qDebug() << "Unlocked file" << m_filePath;
    return true;
}
