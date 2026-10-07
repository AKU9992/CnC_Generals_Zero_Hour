using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

static class Launcher {
    [STAThread] static int Main(string[] args) {
        try {
            string root = AppDomain.CurrentDomain.BaseDirectory;
            bool zh = File.Exists(Path.Combine(root, "Client", "ZeroHour.edition"));
            string exe = Path.Combine(root, "Client", "generals-client.exe");
            if (!File.Exists(exe)) throw new FileNotFoundException("Не найден файл клиента. Повторите установку.", exe);
            var size = Screen.PrimaryScreen.Bounds.Size;
            var start = new ProcessStartInfo(exe) { WorkingDirectory = root, UseShellExecute = false };
            start.Arguments = "-useCwd -nologo";
            if (!zh) start.Arguments += " -win -xres " + size.Width + " -yres " + size.Height;
            start.EnvironmentVariables["GENERALS_RENDERER"] = "d3d12";
            start.EnvironmentVariables["GENERALS_RENDER_FPS"] = "0";
            if (!zh) {
                start.EnvironmentVariables["GENERALS_BORDERLESS"] = "1";
                start.EnvironmentVariables["GENERALS_NATIVE_PRESENT"] = "1";
            }
            using (var game = Process.Start(start)) { game.WaitForExit(); return game.ExitCode; }
        } catch (Exception e) { MessageBox.Show(e.Message, "Generals GPT Mod", MessageBoxButtons.OK, MessageBoxIcon.Error); return 1; }
    }
}
