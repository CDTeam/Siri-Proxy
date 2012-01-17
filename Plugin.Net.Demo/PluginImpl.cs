using System;
using System.Collections.Generic;
using System.Text;
using SiriProxy.Plugins.Interface4Net;
using System.Runtime.InteropServices;

namespace Plugin.Net.Demo
{
    //插件实现
    public static class PluginImpl
    {
        public static bool InitPluginModule()
        {
            //ToDo:这里对模块进行一些初始化的工作
            return true;
        }

        public static void UnInitPluginModule()
        {
            //ToDo:进行一些清理工作等
            //该函数实际上不会得到调用，保留备用
        }

        //返回值的是CommandCallResult的指针
        public static IntPtr OnCommandCall(ref PlistPackageArgs args)
        {
            CommandCallResult rs = default(CommandCallResult);
            rs.FreeCommandCallResult = FreeCommandCallResult;
            rs.type = ResultType.RESULT_TEXT;//告诉SiriProxy,返回的是文本，不带任何plist xml标记的文本
            string text = "caught the command text:";
            text += args.sendText;
            rs.result_text = Marshal.StringToHGlobalAnsi(text);
            IntPtr p = Marshal.AllocHGlobal(Marshal.SizeOf(rs));
            Marshal.StructureToPtr(rs, p, true);
            return p;
        }

        public static void FreeCommandCallResult(IntPtr p)
        {
            CommandCallResult rs = (CommandCallResult)Marshal.PtrToStructure(p, typeof(CommandCallResult));
            if (rs.result_text != IntPtr.Zero)
            {//释放为result_text分配的非托管内存
                Marshal.FreeHGlobal(rs.result_text);
            }
            Marshal.FreeHGlobal(p);//释放为CommandCallResult结构分配的非托管内存
        }

        public static void PluginInfoFree(IntPtr p)
        {
            PluginInfo info = (PluginInfo)Marshal.PtrToStructure(p, typeof(PluginInfo));
            if (info.PluginName != IntPtr.Zero)
            {//释放为PluginName分配的非托管内存
                Marshal.FreeHGlobal(info.PluginName);
            }
            if (info.CommandText != IntPtr.Zero)
            {//释放为CommandText分配的非托管内存
                Marshal.FreeHGlobal(info.CommandText);
            }
            if (info.CommandMatchPattern != IntPtr.Zero)
            {//释放为CommandMatchPattern分配的非托管内存
                Marshal.FreeHGlobal(info.CommandMatchPattern);
            }
            Marshal.FreeHGlobal(p);//释放为PluginInfo结构分配的非托管内存
        }
    }
}
