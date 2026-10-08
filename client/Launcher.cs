using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("AKU9992")]
[assembly: System.Reflection.AssemblyDescription("AKU9992")]
[assembly: System.Reflection.AssemblyCompany("AKU9992")]
[assembly: System.Reflection.AssemblyProduct("AKU9992 Native12")]
static class Launcher {
    [STAThread] static int Main(string[] args) {
        try {
            string root = AppDomain.CurrentDomain.BaseDirectory;
            var fpsSettings=FpsSettings.Load(root);
            if(args.Length>0 && args[0]=="--settings"){Application.EnableVisualStyles();using(var form=new FpsSettingsForm(root,fpsSettings)){form.ShowDialog();}return 0;}
            bool zh = File.Exists(Path.Combine(root, "Client", "ZeroHour.edition"));
            string exe = Path.Combine(root, "Client", "generals-client.exe");
            if (!File.Exists(exe)) throw new FileNotFoundException("Не найден файл клиента. Повторите установку.");
            var size = Screen.PrimaryScreen.Bounds.Size;
            bool native12 = File.Exists(Path.Combine(root, "Client", "generals-native12.dll"));
            var start = new ProcessStartInfo(exe) { WorkingDirectory = root, UseShellExecute = false };
            start.Arguments = "-useCwd -nologo";
            if (!zh || native12) start.Arguments += " -win -xres " + size.Width + " -yres " + size.Height;
            start.EnvironmentVariables["GENERALS_RENDERER"] = native12 ? "native12" : "d3d12";
            if(native12){
                if(string.IsNullOrEmpty(start.EnvironmentVariables["GENERALS_FPS_MODE"]))start.EnvironmentVariables["GENERALS_FPS_MODE"]=fpsSettings.Mode;
                if(string.IsNullOrEmpty(start.EnvironmentVariables["GENERALS_CUSTOM_FPS"]))start.EnvironmentVariables["GENERALS_CUSTOM_FPS"]=fpsSettings.Custom.ToString("R",System.Globalization.CultureInfo.InvariantCulture);
                if(string.IsNullOrEmpty(start.EnvironmentVariables["GENERALS_VSYNC"]))start.EnvironmentVariables["GENERALS_VSYNC"]=fpsSettings.VSync?"1":"0";
                if(string.IsNullOrEmpty(start.EnvironmentVariables["GENERALS_FPS_PROFILE"]) && string.IsNullOrEmpty(start.EnvironmentVariables["GENERALS_TEST_QUIT_SECONDS"]))start.EnvironmentVariables.Remove("GENERALS_RENDER_FPS");
            }else start.EnvironmentVariables["GENERALS_RENDER_FPS"]="0";
            if (native12 && zh) {
                string baseGame = Path.GetFullPath(Path.Combine(root, "..", "CaCG"));
                if (!Directory.Exists(baseGame)) throw new DirectoryNotFoundException("Распакуйте CaCG и CaCGZH рядом: Zero Hour требует ресурсы Generals.");
                start.EnvironmentVariables["GENERALS_BASE_GAME"] = baseGame + Path.DirectorySeparatorChar;
            }
            if (!zh || native12) {
                start.EnvironmentVariables["GENERALS_BORDERLESS"] = "1";
                if (!zh) start.EnvironmentVariables["GENERALS_NATIVE_PRESENT"] = "1";
            }
            using (var game = Process.Start(start)) { game.WaitForExit(); return game.ExitCode; }
        } catch (Exception) { MessageBox.Show("Не удалось запустить клиент. Проверьте файлы установки и наличие DirectX 12.", "AKU9992", MessageBoxButtons.OK, MessageBoxIcon.Error); return 1; }
    }
}

sealed class FpsSettings {
    public string Mode="Standard";
    public double Custom=60;
    public bool VSync=false;
    public static FpsSettings Load(string root) {
        var result=new FpsSettings(); string path=Path.Combine(root,"Client","AKU9992-settings.ini");
        if(!File.Exists(path))return result;
        foreach(var line in File.ReadAllLines(path)) {
            int equal=line.IndexOf('=');if(equal<0)continue;
            string key=line.Substring(0,equal).Trim(),value=line.Substring(equal+1).Trim();
            if(key=="FpsMode" && (value=="Standard" || value=="Auto" || value=="Custom"))result.Mode=value;
            double custom;
            if(key=="CustomFPS" && double.TryParse(value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out custom) && custom>=1 && custom<=1000)result.Custom=custom;
            if(key=="VSync")result.VSync=value=="1";
        }
        return result;
    }
    public void Save(string root) {
        File.WriteAllText(Path.Combine(root,"Client","AKU9992-settings.ini"),"[Renderer]\r\nFpsMode="+Mode+"\r\nCustomFPS="+Custom.ToString("R",System.Globalization.CultureInfo.InvariantCulture)+"\r\nVSync="+(VSync?"1":"0")+"\r\n");
    }
}
sealed class FpsSettingsForm : Form {
    [System.Runtime.InteropServices.DllImport("kernel32.dll",CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
    static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
    [System.Runtime.InteropServices.DllImport("kernel32.dll")] static extern IntPtr GetProcAddress(IntPtr module,string name);
    [System.Runtime.InteropServices.DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)]
    delegate double RateFn(IntPtr window);
    [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int ExactFn(IntPtr window);
    public FpsSettingsForm(string root,FpsSettings settings) {
        Text="AKU9992 — настройки FPS";ClientSize=new System.Drawing.Size(550,285);StartPosition=FormStartPosition.CenterScreen;
        FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;Font=new System.Drawing.Font("Segoe UI",10);
        double refresh=60;bool exact=false;IntPtr module=LoadLibraryEx(Path.Combine(root,"Client","generals-native12.dll"),IntPtr.Zero,0x100|0x800);
        if(module!=IntPtr.Zero) {
            try {
                IntPtr fn=GetProcAddress(module,"generalsNativeGetDisplayRate");
                IntPtr valid=GetProcAddress(module,"generalsNativeDisplayRateExact");if(valid!=IntPtr.Zero)exact=((ExactFn)System.Runtime.InteropServices.Marshal.GetDelegateForFunctionPointer(valid,typeof(ExactFn)))(Handle)==1;
                if(fn!=IntPtr.Zero)refresh=((RateFn)System.Runtime.InteropServices.Marshal.GetDelegateForFunctionPointer(fn,typeof(RateFn)))(Handle);
            }finally{FreeLibrary(module);}
        }
        var display=new Label {Text="Активная частота экрана: "+refresh.ToString("0.###",System.Globalization.CultureInfo.InvariantCulture)+(exact?" Гц":" Гц (безопасный резерв)"),AutoSize=true,Location=new System.Drawing.Point(20,20)};
        var mode=new ComboBox {DropDownStyle=ComboBoxStyle.DropDownList,Location=new System.Drawing.Point(20,60),Width=500};
        mode.Items.AddRange(new object[]{"Standard — 60 FPS (или ниже частоты экрана)","Auto — текущая частота экрана","Custom — своё ограничение"});
        mode.SelectedIndex=settings.Mode=="Auto"?1:settings.Mode=="Custom"?2:0;
        var fps=new NumericUpDown {Minimum=1,Maximum=1000,DecimalPlaces=3,Increment=1,Value=(decimal)settings.Custom,Location=new System.Drawing.Point(20,104),Width=130,Enabled=mode.SelectedIndex==2};
        mode.SelectedIndexChanged+=(s,e)=>fps.Enabled=mode.SelectedIndex==2;
        var note=new Label {Text="Custom автоматически ограничивается частотой активного экрана.\nВ фоне действует предел 30 FPS. Скорость игры независима от FPS.",AutoSize=true,Location=new System.Drawing.Point(20,145)};
        var sync=new CheckBox {Text="VSync",Checked=settings.VSync,AutoSize=true,Location=new System.Drawing.Point(180,107)};
        var save=new Button {Text="Сохранить",Location=new System.Drawing.Point(280,235),Width=120,DialogResult=DialogResult.OK};
        var cancel=new Button {Text="Отмена",Location=new System.Drawing.Point(410,235),Width=120,DialogResult=DialogResult.Cancel};
        save.Click+=(s,e)=>{settings.Mode=mode.SelectedIndex==1?"Auto":mode.SelectedIndex==2?"Custom":"Standard";settings.Custom=(double)fps.Value;settings.VSync=sync.Checked;settings.Save(root);};
        Controls.AddRange(new Control[]{display,mode,fps,note,sync,save,cancel});AcceptButton=save;CancelButton=cancel;
    }
}