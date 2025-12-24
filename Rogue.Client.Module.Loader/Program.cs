using Reloaded.Injector;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Rogue.Client.Module.Loader
{
    class Program
    {
        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll")]
        private static extern int GetWindowTextLength(IntPtr hWnd);

        static RogueGameProcess RogueClient = null;

        static void Main()
        {
            Console.WriteLine("Monitoring TheDivision.exe...");

            Icon appIcon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);

            NotifyIcon trayIcon = new NotifyIcon
            {
                Icon = appIcon,
                Visible = true,
                Text = "Rogue.Client.Watchdog"
            };

            ContextMenuStrip contextMenu = new ContextMenuStrip();
            contextMenu.Items.Add("Exit", null, (s, e) =>
            {
                trayIcon.Visible = false;
                Application.Exit();
            });
            trayIcon.ContextMenuStrip = contextMenu;

            System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer
            {
                Interval = 2000
            };
            timer.Tick += Timer_Tick;
            timer.Start();

            Application.Run();
        }

        private static void Timer_Tick(object sender, EventArgs e)
        {
            if (RogueClient != null)
            {
                if (RogueClient.Process.HasExited)
                {
                    Console.WriteLine($"[{DateTime.Now}] TheDivision.exe closed.");
                    RogueClient = null;
                }
                else
                {
                    return;
                }
            }

            var processes = Process.GetProcessesByName("TheDivision");
            foreach (var process in processes)
            {
                string windowTitle = GetMainWindowTitle(process);
                if (windowTitle == "Tom Clancy's The Division")
                {
                    RogueClient = new RogueGameProcess
                    {
                        Process = process,
                        WindowTitle = windowTitle
                    };

                    Console.WriteLine($"[{DateTime.Now}] Found TheDivision.exe running.");
                    OnGameFound();
                    break;
                }
            }
        }

        private static string GetMainWindowTitle(Process process)
        {
            try
            {
                IntPtr handle = process.MainWindowHandle;
                if (handle == IntPtr.Zero) return null;

                int length = GetWindowTextLength(handle);
                var sb = new System.Text.StringBuilder(length + 1);
                GetWindowText(handle, sb, sb.Capacity);
                return sb.ToString();
            }
            catch
            {
                return null;
            }
        }

        private static void OnGameFound()
        {
            string dllPath = Path.Combine(AppContext.BaseDirectory, "Division_QOL_Tools.dll");


            Thread.Sleep(1500);
            var injector = new Injector(RogueClient.Process);
            injector.Inject(dllPath);
        }
    }

    class RogueGameProcess
    {
        public Process Process { get; set; }
        public string WindowTitle { get; set; }
    }
}
