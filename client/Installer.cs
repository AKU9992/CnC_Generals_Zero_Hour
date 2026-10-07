using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

// Seekable archive slice keeps ZIP offsets relative to the embedded payload.
sealed class PayloadStream : Stream {
    readonly FileStream file; readonly long origin, length; long position;
    public PayloadStream(FileStream file, long origin, long length) { this.file=file; this.origin=origin; this.length=length; }
    public override bool CanRead { get { return true; } }
    public override bool CanSeek { get { return true; } }
    public override bool CanWrite { get { return false; } }
    public override long Length { get { return length; } }
    public override long Position { get { return position; } set { Seek(value, SeekOrigin.Begin); } }
    public override int Read(byte[] bytes, int offset, int count) {
        count=(int)Math.Min(count,length-position); if(count==0) return 0;
        file.Position=origin+position; int read=file.Read(bytes,offset,count); position+=read; return read;
    }
    public override long Seek(long offset, SeekOrigin kind) {
        long next=kind==SeekOrigin.Begin ? offset : kind==SeekOrigin.Current ? position+offset : length+offset;
        if(next<0 || next>length) throw new IOException("Invalid payload seek."); return position=next;
    }
    public override void Flush() {}
    public override void SetLength(long value) { throw new NotSupportedException(); }
    public override void Write(byte[] bytes,int offset,int count) { throw new NotSupportedException(); }
    protected override void Dispose(bool disposing) { if(disposing) file.Dispose(); base.Dispose(disposing); }
}

static class InstallEngine {
    public static ZipArchive Open() {
        var file=File.OpenRead(Application.ExecutablePath);
        try {
            if(file.Length<24) throw new InvalidDataException("Установочный архив отсутствует.");
            file.Position=file.Length-24;
            var reader=new BinaryReader(file,Encoding.UTF8,true);
            long origin=reader.ReadInt64(), size=reader.ReadInt64();
            string magic=Encoding.ASCII.GetString(reader.ReadBytes(8));
            if(magic!="GGPTPK01" || origin<0 || size<0 || origin>file.Length-24 || size!=file.Length-24-origin)
                throw new InvalidDataException("Установочный архив повреждён.");
            return new ZipArchive(new PayloadStream(file,origin,size),ZipArchiveMode.Read,false);
        } catch { file.Dispose(); throw; }
    }
    public static string Hash(Stream stream) {
        using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","");
    }
    static List<string[]> Manifest(ZipArchive archive) {
        var entry=archive.GetEntry("release.manifest");
        if(entry==null) throw new InvalidDataException("Нет списка файлов клиента.");
        var files=new List<string[]>(); var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using(var reader=new StreamReader(entry.Open())) {
            string line; while((line=reader.ReadLine())!=null) {
                var fields=line.Split('\t'); long size;
                if(fields.Length!=3 || fields[0].Length!=64 || !long.TryParse(fields[1],out size) || size<0 || !names.Add(fields[2]))
                    throw new InvalidDataException("Некорректный список файлов.");
                if(!fields[2].StartsWith("CaCG/",StringComparison.Ordinal) && !fields[2].StartsWith("CaCGZH/",StringComparison.Ordinal))
                    throw new InvalidDataException("Некорректная папка клиента.");
                files.Add(fields);
            }
        }
        if(files.Count==0 || archive.Entries.Count!=files.Count+1) throw new InvalidDataException("Неполный архив клиента.");
        return files;
    }
    static string Destination(string root,string relative) {
        string path=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar)));
        if(!path.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Недопустимый путь в архиве.");
        return path;
    }
    public static void Run(string directory,bool verify,bool shortcuts,Action<int,string> progress) {
        string root=verify ? null : Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar);
        if(!verify) {
            foreach(var name in new[] {"generals-client","generals","generals-x64-test","generalszh-x64-test"})
                foreach(var process in Process.GetProcessesByName(name)) {
                    using(process) {
                        string exe=null; try { exe=process.MainModule.FileName; } catch {}
                        if(exe!=null && exe.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
                            throw new IOException("Закройте игру перед установкой.");
                    }
                }
            Directory.CreateDirectory(root);
        }
        using(var archive=Open()) {
            var files=Manifest(archive); long total=0,done=0;
            foreach(var fields in files) total+=long.Parse(fields[1]);
            foreach(var fields in files) {
                string relative=fields[2]; var entry=archive.GetEntry(relative);
                if(entry==null || entry.Length!=long.Parse(fields[1])) throw new InvalidDataException("Неполный файл: "+relative);
                string destination=verify ? null : Destination(root,relative);
                string temporary=verify ? null : destination+".install-"+Guid.NewGuid().ToString("N");
                try {
                    string actual;
                    if(verify) { using(var input=entry.Open()) actual=Hash(input); }
                    else {
                        Directory.CreateDirectory(Path.GetDirectoryName(destination));
                        using(var input=entry.Open()) using(var output=File.Create(temporary)) input.CopyTo(output);
                        using(var input=File.OpenRead(temporary)) actual=Hash(input);
                    }
                    if(!actual.Equals(fields[0],StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Не совпадает контрольная сумма: "+relative);
                    if(!verify) {
                        for(int retry=0;;retry++) {
                            try {
                                if(File.Exists(destination)) File.Delete(destination);
                                File.Move(temporary,destination); break;
                            } catch(IOException) {
                                if(retry>=31) throw; Thread.Sleep(250);
                            }
                        }
                    }
                } finally { if(temporary!=null && File.Exists(temporary)) File.Delete(temporary); }
                done+=entry.Length; if(progress!=null) progress((int)(total==0?100:done*100/total),relative);
            }
        }
        if(!verify && shortcuts) CreateShortcuts(root);
    }
    static void CreateShortcuts(string root) {
        dynamic shell=Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell"));
        var games=new[] {"CaCG","CaCGZH"};
        var titles=new[] {"Command and Conquer Generals","Command and Conquer Generals Zero Hour [MOD SymBioz]"};
        for(int i=0;i<games.Length;i++) {
            string target=Path.Combine(root,games[i],"generals.exe"), desktop=Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            string existing=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory),titles[i]+".lnk");
            string shortcut=File.Exists(existing)?existing:Path.Combine(desktop,titles[i]+".lnk");
            try { SaveShortcut(shell,shortcut,target); }
            catch { SaveShortcut(shell,Path.Combine(desktop,titles[i]+".lnk"),target); }
        }
    }
    static void SaveShortcut(dynamic shell,string path,string target) {
        dynamic shortcut=shell.CreateShortcut(path); shortcut.TargetPath=target; shortcut.Arguments="";
        shortcut.WorkingDirectory=Path.GetDirectoryName(target); shortcut.IconLocation=target+",0"; shortcut.Save();
    }
}

sealed class InstallerForm : Form {
    readonly TextBox folder=new TextBox(); readonly Button install=new Button(),browse=new Button();
    readonly Label status=new Label(); readonly ProgressBar progress=new ProgressBar(); bool busy,installed;
    public InstallerForm() {
        Text="Generals GPT Mod — установка"; ClientSize=new Size(620,270); FormBorderStyle=FormBorderStyle.FixedDialog;
        MaximizeBox=false; StartPosition=FormStartPosition.CenterScreen; Font=new Font("Segoe UI",10);
        var title=new Label {Text="C&C Generals + Zero Hour · изменённый клиент x64",AutoSize=true,Location=new Point(20,20)};
        var hint=new Label {Text="Выберите общую папку для CaCG и CaCGZH.\nПосле установки используйте ярлыки на рабочем столе.",AutoSize=true,Location=new Point(20,55)};
        folder.SetBounds(20,110,480,30); folder.Text=Directory.Exists(@"E:\C&C ZH GPTMOD")?@"E:\C&C ZH GPTMOD":Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Generals GPT Mod");
        browse.Text="Обзор…"; browse.SetBounds(510,110,90,30); browse.Click+=(s,e)=>{using(var dialog=new FolderBrowserDialog()){dialog.SelectedPath=folder.Text;if(dialog.ShowDialog(this)==DialogResult.OK)folder.Text=dialog.SelectedPath;}};
        status.SetBounds(20,150,580,40); status.Text="Готов к установке. Сохранения и настройки игрока остаются в Документах.";
        progress.SetBounds(20,195,580,18); install.Text="Установить"; install.SetBounds(460,225,140,30);
        install.Click+=async (s,e)=>await StartInstallation(); Controls.AddRange(new Control[]{title,hint,folder,browse,status,progress,install});
        FormClosing+=(s,e)=>{if(busy)e.Cancel=true;};
    }
    async Task StartInstallation() {
        if(installed) { Close(); return; }
        busy=true; install.Enabled=browse.Enabled=folder.Enabled=false;
        try {
            string destination=folder.Text;
            await Task.Run(()=>InstallEngine.Run(destination,false,true,(value,name)=>BeginInvoke(new Action(()=>{progress.Value=value;status.Text="Установка: "+name;}))));
            status.Text="Установлено. Запускайте Generals и Zero Hour с ярлыков рабочего стола.";
            installed=true; install.Text="Готово";
        } catch(Exception e) {status.Text="Установка не завершена.";MessageBox.Show(this,e.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Error);}
        finally {busy=false; install.Enabled=browse.Enabled=folder.Enabled=true;}
    }
}

static class Setup {
    [STAThread] static int Main(string[] args) {
        try {
            if(args.Length>=1 && args[0]=="--verify") {InstallEngine.Run(null,true,false,null);return 0;}
            if(args.Length>=2 && args[0]=="--extract") {InstallEngine.Run(args[1],false,false,null);return 0;}
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new InstallerForm());return 0;
        } catch(Exception e) {
            if(args.Length>0) File.WriteAllText(Path.Combine(Path.GetDirectoryName(Application.ExecutablePath),"setup-error.log"),e.ToString());
            else MessageBox.Show(e.Message,"Generals GPT Mod",MessageBoxButtons.OK,MessageBoxIcon.Error);
            return 1;
        }
    }
}
