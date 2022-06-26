#include <string.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/sched.h>

long sys_getcwd(char* buf, size_t size)
{
    struct m_inode* inode = current->pwd; //当前目录i节点
    struct buffer_head* path_head = bread(current->root->i_dev, inode->i_zone[0]);// 读取当前数据块
    struct dir_entry* getpath = (struct dir_entry*)path_head->b_data; //该数据块下第一个目录项即目录本身
    unsigned short pre_inode;
    struct m_inode* new_inode;
    char* name[256]; 
    char* pname;
    int i = 0;
    while (1) {
        pre_inode = getpath->inode;//保存当前目录i节点以作查找用
        new_inode = iget(current->root->i_dev, (getpath + 1)->inode);//查找上级目录
        path_head = bread(current->root->i_dev, new_inode->i_zone[0]);
        getpath = (struct dir_entry*)path_head->b_data;
        int j = 1;
        while ((getpath + j)->inode ！= pre_inode) //遍历上级目录找本级目录
            j++;
        if (j == 1)//已经是根节点了
            break;
        name[i] = (getpath + j)->name;
        i++;
    }
    int count = 0;
    i--;
    int k;
    if (i < 0)
        return NULL;
    while (i >= 0)//整理路径名
    {
        k = 0;
        pname[count++] = '/';
        while (name[i][k] != '\0')
        {
            pname[count] = name[i][k];
            k++;
            count++;
        }
        i--;
    }
    if (count < 0)
        return NULL;
    for (k = 0; k < count; k++)//输出
        put_fs_byte(pname[k], buf + k);
    return (long)(pname);
}

int sys_getdents(unsigned int fd, struct linux_dirent* dirp, unsigned int count)
{
    struct linux_dirent lastdirent;
    struct m_inode* inode = current->filp[fd]->f_inode;//获取文件i节点
    struct buffer_head* path_head = bread(inode->i_dev, inode->i_zone[0]);//同getcwd
    struct dir_entry* path = (struct dir_entry*)path_head->b_data;//同getcwd
    int ct= 0;
    int i, j, k;
    for (i = 0; i < 1024; i++)//每次循环输出一个目录项
    {
        if (path->inode == 0 || (i + 1) * 24 > count) 
            break;
        lastdirent.d_ino = path[i].inode;
        for (j = 0; j < 14; j++)//将目前目录项名字保存在d_name中
            lastdirent.d_name[j] = path[i].name[j];
        lastdirent.d_off = 0;
        lastdirent.d_reclen = 24;
        for (k = 0; k < 24; k++) {
            put_fs_byte(((char*)&lastdirent)[k], ((char*)dirp + ct));
            ct++;
        }
    }
    if (ct== 24)
        return 0;
    return ct;
}
