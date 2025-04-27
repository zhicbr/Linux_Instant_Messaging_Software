#ifndef FILELOCK_H
#define FILELOCK_H

#include <QObject>
#include <fcntl.h>
#include <unistd.h>
#include <QString>

class FileLock : public QObject {
    Q_OBJECT
public:
    enum LockType {
        ReadLock,
        WriteLock
    };

    explicit FileLock(QObject *parent = nullptr);
    ~FileLock();

    // 打开文件
    bool open(const QString &filePath);
    
    // 关闭文件
    bool close();
    
    // 加锁
    bool lock(LockType type, bool wait = true);
    
    // 解锁
    bool unlock();
    
    // 检查文件是否已打开
    bool isOpen() const { return m_fd != -1; }

private:
    int m_fd;           // 文件描述符
    QString m_filePath; // 文件路径
};

#endif // FILELOCK_H
