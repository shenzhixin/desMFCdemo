#include "StdAfx.h"
#include "task.h"
#include <ImageHlp.h>

string key;         //密钥
string en_dir;      //加密输出目录
string de_dir;      //解密输出目录
Fvector filelist;   //文件列表
int alg;            //加解密算法类型
int type;           //操作类型，0-一级加密，1-二级加密，2-解密

CRITICAL_SECTION g_cs;    //临界区对象
int g_index;          //全局资源，记录文件列表中已处理的索引
static HWND g_mainHnd;    //父窗体句柄
static HANDLE g_Hnd_1;   //线程一句柄
static HANDLE g_Hnd_2;   //线程二句柄


//构造函数,初始化成员变量
//	 key_f       密钥
//   en_dir_f    加密输出目录
//   de_dir_f    解密输出目录
//   filelist_f  文件列表
//	 mainHwnd    父窗体句柄
task::task(HWND mainHwnd, string key_f, Fvector filelist_f, string en_dir_f = "", string de_dir_f = "")
{
	g_mainHnd = mainHwnd;   //父窗体句柄
	key = key_f;            //密钥
	en_dir = en_dir_f;      //加密输出目录
	de_dir = de_dir_f;      //解密输出目录
	filelist = filelist_f;  //文件列表	
}


//析构函数
task::~task(void)
{
	//释放存放文件列表容器所占内存
	//Fvector().swap(filelist);  
}


//加密函数
// alg		加解密算法类型，0-DES，1-RC4，
// type		操作类型，0-加密，1-解密
void task::DoTask(int alg_f, int type_f)
{
	//加解密算法类型赋值
	alg = alg_f;
	//操作类型赋值
	type = type_f;
	//初始化临界区对象
	InitializeCriticalSection(&g_cs);
	//初始化临界区资源
	g_index = 0;

	//定义两个线程用于加密操作
	CWinThread *pThread_1, *pThread_2;

	pThread_1 = AfxBeginThread(EnThread, this);   //开始加密线程1
	g_Hnd_1 = pThread_1 ->m_hThread;            //获取线程1句柄

	pThread_2 = AfxBeginThread(EnThread, this);   //开始加密线程2
	g_Hnd_2 = pThread_2 ->m_hThread;            //获取线程2句柄
}


//文件路径转换函数
//  dir        输出路径
//  fileinfo   待处理文件信息
//  dir_temp   转换后文件路径
void pathTransform(string dir, File fileinfo, string &dir_temp)
{
		if(dir != "")  
		{
			//将输出目录赋值给临时变量
			dir_temp = dir;
			//查找要保持的文件目录结构起始位置
			int pos = fileinfo.from.size();
			
			if(pos == 0)    //是选择文件操作
			{
				//找到路径中文件名起始位置
				pos = fileinfo.file.find_last_of('\\');
				//添加文件名
				dir_temp += fileinfo.file.substr(pos);
			}
			else			//是选择目录操作
			{
				//添加文件目录结构及文件名
				dir_temp += fileinfo.file.substr(pos);
				//保持文件的目录结构，如果目录不存在则创建目录
				MakeSureDirectoryPathExists(dir_temp.c_str());
			}
		}
		else
		{
			//输出目录为空则在源目录生成重命名解密文件
			dir_temp = fileinfo.file;
		}
		//修改加密文件扩展名
		int posext = dir_temp.find_last_of('.');
		//去除原文件的拓展名
		dir_temp = dir_temp.substr(0, posext);
		//添加定义好的加密文件拓展名
		if(type == 2)  //解密操作时拓展名为.fess
			dir_temp += ".fess";
		else
			dir_temp += ".fes";
}


//加解密线程函数
UINT EnThread(LPVOID param)
{
	//线程当前要处理的文件列表索引
	int index;

	while(1)
	{	
		//临界区锁定共享资源
		EnterCriticalSection(&g_cs);
        
		if(g_index == filelist.size())
		{
			//向主线程发送完成消息
			::PostMessage(g_mainHnd, WM_FINISHMSG, 0, 0);
			//临界区解锁
			LeaveCriticalSection(&g_cs);
			return false;      //退出函数
		}
		//获得共享区索引值,并进行加1操作
		index = g_index++;
		//临界区解锁
		LeaveCriticalSection(&g_cs);

		//输出目录临时变量
		string dir_temp;

		//创建算法基类对象
		BaseAlg *base = NULL;

		switch(type)
		{
		case iEncrypt:		//一级加密
			//转换成相应算法类对象
			base = CreateAlg(base, alg);
		switch(type)
		case 0:		//一级加密
			//调用文件路径转换函数
			pathTransform(en_dir, filelist[index], dir_temp);
			//调用加密函数
			base->Encrypt(filelist[index].file, key.c_str(), dir_temp);
			break;

		case  iSecEncrypt:	//二级加密
			//转换成相应算法类对象
			base = CreateAlg(base, alg);
			//调用文件路径转换函数
			pathTransform(en_dir, filelist[index], dir_temp);
			//调用二级加密函数
			base->SecondEncrypt(filelist[index].file, key.c_str(), dir_temp);
			break;

		case iDecrypt:		//解密
			{
				//待解密文件加密类型
				int alg_temp;
				//待解密文件原扩展名
				string ext_temp = "";
				//读取待解密文件信息
				ReadInfo(alg_temp, ext_temp, filelist[index].file);

				//转换成相应算法类对象
				base = CreateAlg(base, alg_temp);
				//调用文件路径转换函数
				pathTransform(de_dir, filelist[index], dir_temp);

				//更改文件解密输出文件扩展名为文件原扩展名
				int posext = dir_temp.find_last_of('.');
				//去除原文件的拓展名
				dir_temp = dir_temp.substr(0, posext + 1);
				dir_temp += ext_temp;
				//调用解密函数
				base->Decrypt(filelist[index].file, key.c_str(), dir_temp);
				break;
			}
		default:
			break;
		}
	}
	return true;
}

//算法类转换函数
// base		算法基类对象
// alg		加解密算法类型
BaseAlg *CreateAlg(BaseAlg *base, int alg)
{
	switch(alg)
	{
		//DES加解密
	case iDES:
		base = new DESAlg;
		break;
		//RC4加解密
	case iRC4:
		//base = new RC4Alg;
		break;
	case iAES:
		//AES加解密
		//base = new AESAlg;
		break;
	default:
		break;
	}
	//返回对象指针
	return base;
}