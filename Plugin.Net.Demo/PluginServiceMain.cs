using System;
using System.Collections.Generic;
using System.Text;
using SiriProxy.Plugins.Interface4Net;
using System.Runtime.InteropServices;

namespace Plugin.Net.Demo
{
    public class PluginServiceMain:IPluginServiceMain
    {
        #region IPluginServiceMain 成员
        //显式接口实现
        PluginInfo IPluginServiceMain.PluginServiceMain()
        {
            //return CustomGetPluginInfo();

            string commandText = "xxxx";
            string commandPattern = ".+";//使用正则对命令匹配,正则匹配优先级高于commandText

            PluginServiceImpl plugin= PluginServiceImpl.Create("595D892B-FFC1-4868-86B6-0F49ACD9FB60", "The siriproxy plugin demo implements by c# language!",commandText,commandPattern);

            plugin.CommandCallEvent += new SiriProxy.Plugins.Interface4Net.EventHandler<EventArgs<CommandCallArgs>>(plugin_CommandCallEvent);
            plugin.PluginInitEvent += new SiriProxy.Plugins.Interface4Net.EventHandler<EventArgs<bool>>(plugin_PluginInitEvent);
            plugin.PluginUnInitEvent += new EventHandler(plugin_PluginUnInitEvent);
            return plugin.PluginInfo;
        }

        void plugin_PluginUnInitEvent(object sender, EventArgs e)
        {
            //ToDo:该委托现有版本不会得到调用,保留
        }

        void plugin_PluginInitEvent(object sender, EventArgs<bool> args)
        {
            //ToDo:这里对插件进行一些初始化
            args.Data = true;//作为演示的目的，这里只是简单地将Data字段置为true，告诉调用方初始化成功
        }

        void plugin_CommandCallEvent(object sender, EventArgs<CommandCallArgs> args)
        {
            //ToDo:写一些实际性的业务逻辑，返回结果赋值到args.Data.ResultText
            args.Data.ResultText = string.Format("the response has filter by .net plugin,response text is:{0}",args.Data.ResponseText);
            
        }

        //不适用提供的封装类，自己控制所有细节，请参照如下方式
        private PluginInfo CustomGetPluginInfo()
        {
            PluginInfo info = default(PluginInfo);
            byte[] idarr = Encoding.Default.GetBytes("CFA8D34D-119D-442f-AEA1-45304DE4FD37");
            info.PluginId = new byte[37];
            Buffer.BlockCopy(idarr, 0, info.PluginId, 0, idarr.Length);
            info.PluginId[36] = (byte)'\0';
            string name = "The siriproxy plugin demo implements by c# language!";
            info.PluginName = Marshal.StringToHGlobalAnsi(name);
            info.InitPluginModule = PluginImpl.InitPluginModule;
            info.UnInitPluginModule = PluginImpl.UnInitPluginModule;
            info.OnCommandCall = PluginImpl.OnCommandCall;
            info.PluginInfoFree = PluginImpl.PluginInfoFree;
            info.CommandText = Marshal.StringToHGlobalAnsi("Zero");
            info.CommandMatchPattern = IntPtr.Zero;
            return info;
        }
        #endregion
    }
}
