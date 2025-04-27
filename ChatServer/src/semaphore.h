#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <QObject>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <QString>

class Semaphore : public QObject {
    Q_OBJECT
public:
    explicit Semaphore(QObject *parent = nullptr);
    ~Semaphore();

    // 创建或打开命名信号量
    bool create(const QString &name, unsigned int value = 1);
    
    // 关闭信号量
    bool close();
    
    // 等待信号量（P操作）
    bool wait(int timeout_ms = -1);
    
    // 释放信号量（V操作）
    bool post();
    
    // 获取信号量的值
    int getValue();
    
    // 检查信号量是否有效
    bool isValid() const { return m_sem != SEM_FAILED; }

private:
    sem_t *m_sem;      // 信号量指针
    QString m_name;    // 信号量名称
};

#endif // SEMAPHORE_H
