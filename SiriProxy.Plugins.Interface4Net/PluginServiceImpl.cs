﻿// Copyright (c) 2011,cd-team.org.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace SiriProxy.Plugins.Interface4Net
{
    public class PluginServiceImpl
    {
        private static Dictionary<string, PluginServiceImpl> maps =new Dictionary<string, PluginServiceImpl>();
        private string _key = null;
        PluginInfo _info;

        private PluginServiceImpl() 
        {
            _key = Guid.NewGuid().ToString();
        }
        ~PluginServiceImpl()
        {
            if (_info.PluginName != IntPtr.Zero)
            {//释放为PluginName分配的非托管内存
                Marshal.FreeHGlobal(_info.PluginName);
            }
            if (_info.CommandText != IntPtr.Zero)
            {//释放为CommandText分配的非托管内存
                Marshal.FreeHGlobal(_info.CommandText);
            }
            if (_info.CommandMatchPattern != IntPtr.Zero)
            {//释放为CommandMatchPattern分配的非托管内存
                Marshal.FreeHGlobal(_info.CommandMatchPattern);
            }
        }

        public static PluginServiceImpl Create(string pluginId,string pluginName,string commandText,string commandPattern)
        {
            PluginServiceImpl obj = new PluginServiceImpl();
            byte[] cache = Encoding.Default.GetBytes(pluginId);
            if (cache.Length > 36 || cache.Length == 0)
            {
                throw new Exception("参数错误:pluginId长度应该大于0且小于37");
            }
            obj._info.PluginId=new byte[37];
            Buffer.BlockCopy(cache, 0, obj._info.PluginId, 0, cache.Length);
            obj._info.PluginName = Marshal.StringToHGlobalAnsi(pluginName);
            if (string.IsNullOrEmpty(commandText))
            {
                throw new ArgumentException("参数commandText不能为空");
            }
            obj._info.CommandText = Marshal.StringToHGlobalAnsi(commandText);
            if (!string.IsNullOrEmpty(commandPattern))
            {
                obj._info.CommandMatchPattern = Marshal.StringToHGlobalAnsi(commandPattern);
            }
            obj._info.InitPluginModule = new PluginFuncInitialize(obj.InitPluginModule);
            obj._info.OnCommandCall = new PluginFuncCommandCall(obj.OnCommandCall);
            obj._info.PluginInfoFree = new FuncPluginInfoFree(obj.PluginInfoFree);
            obj._info.UnInitPluginModule = new PluginFuncUnInitialize(obj.UnInitPluginModule);

            maps[obj._key] = obj;//添加到map，增加对obj的引用，防止GC对该实例进行垃圾回收
            return obj;
        }

        public PluginInfo PluginInfo
        {
            get { return _info; }
        }

        /// <summary>
        /// 当插件初始化时触发该事件
        /// </summary>
        public event EventHandler<EventArgs<bool>> PluginInitEvent;

        /// <summary>
        /// 当插件释放时触发,该事件在当前版本不会得到调用，仅作保留
        /// </summary>
        public event EventHandler PluginUnInitEvent;

        public event EventHandler<EventArgs<CommandCallArgs>> CommandCallEvent;

        #region //Plugin Callbacks
        public bool InitPluginModule()
        {
            if (PluginInitEvent != null)
            {
                //广播初始化事件
                EventArgs<bool> args = new EventArgs<bool>();
                PluginInitEvent(this,args);
                return args.Data;
            }

            //如果没有对象订阅过初始化事件，那么默认返回true
            return true;
        }

        public void UnInitPluginModule()
        {
            //该函数实际上不会得到调用，保留备用
            if (PluginUnInitEvent != null)
            {
                EventArgs args = new EventArgs();
                PluginUnInitEvent(this,args);
            }
        }

        //返回值的是CommandCallResult的指针
        public IntPtr OnCommandCall(ref PlistPackageArgs args)
        {
            CommandCallResult rs = default(CommandCallResult);
            rs.FreeCommandCallResult = FreeCommandCallResult;
            rs.type = ResultType.RESULT_TEXT;//告诉SiriProxy,返回的是文本，不带任何plist xml标记的文本
            string text = null;
            if (CommandCallEvent != null)
            {
                EventArgs<CommandCallArgs> arg = new EventArgs<CommandCallArgs>((CommandCallArgs)args);
                CommandCallEvent(this, arg);
                if (!string.IsNullOrEmpty(arg.Data.ResultText))
                {
                    text = arg.Data.ResultText;
                }
                rs.type = arg.Data.Type;
            }
            if (null == text)
            {
                text = StringHelper.PtrToStringAnsi(args.responseText);//Marshal.PtrToStringAnsi(args.responseText);
            }
            rs.result_text = Marshal.StringToHGlobalAnsi(text);
            
            IntPtr p = Marshal.AllocHGlobal(Marshal.SizeOf(rs));
            Marshal.StructureToPtr(rs, p, true);
            return p;
        }

        public void FreeCommandCallResult(IntPtr p)
        {
            CommandCallResult rs = (CommandCallResult)Marshal.PtrToStructure(p, typeof(CommandCallResult));
            if (rs.result_text != IntPtr.Zero)
            {//释放为result_text分配的非托管内存
                Marshal.FreeHGlobal(rs.result_text);
            }
            Marshal.FreeHGlobal(p);//释放为CommandCallResult结构分配的非托管内存
        }

        public void PluginInfoFree(IntPtr p)
        {
            Marshal.FreeHGlobal(p);//释放为PluginInfo结构分配的非托管内存
        }
        #endregion
    }
}
