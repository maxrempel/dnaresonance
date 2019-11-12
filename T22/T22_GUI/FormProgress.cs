using Protocodes;
using Serilog;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace T22_GUI
{
    public partial class FormProgress : Form, IProgress
    {
        private string folder = Environment.CurrentDirectory;
        private FolderBrowserDialog fb = new FolderBrowserDialog();
        private IProgress progress = new SilentProgress();

        void InitLog()
        {
            string logfilepath = Path.Combine(folder, "Log", "log-.txt");
            Log.Logger = new LoggerConfiguration()
                .WriteTo.File(logfilepath, rollingInterval: RollingInterval.Day)
                .CreateLogger();
        }

        public FormProgress()
        {
            InitializeComponent();

            InitLog();

            string msg = $"Input in folder {folder}";
            labelFolder.Text = msg;
            Log.Information("--");
            Log.Information(msg);
        }

        private void ButtonFolder_Click(object sender, EventArgs e)
        {
            if (fb.ShowDialog() != DialogResult.OK)
            {
                return;
            }
            folder = fb.SelectedPath;

            var msg = $"Input in folder {folder}";
            labelFolder.Text = msg;
            Log.Information(msg);
        }

        private void ButtonStart_Click(object sender, EventArgs e)
        {
            labelDone.Visible = false;
            buttonFolder.Enabled = false;
            buttonStart.Enabled = false;

            Log.Information($"Folder {folder} started");

            progress = this;

            var processed = new TopStopWork(progress)
                .WithFolder(folder)
                .ProcessFolder();

            var msg = $"Folder {folder} ended";
            if (!processed)
            {
                msg += " with errors";
                labelDone.Text = "DONE, with errors";
            }
            else
            {
                labelDone.Text = "DONE";
            }
            Log.Information(msg);

            labelDone.Visible = true;
            buttonFolder.Enabled = true;
            buttonStart.Enabled = true;
        }

        void IProgress.End()
        {
            progressBar1.Visible = false;
            labelFilename.Text = "";
        }

        void IProgress.ReportFile(string filename)
        {
            var msg = $"Processing file {filename}";
            Log.Information(msg);
            labelFilename.Text = msg;
            labelFilename.Visible = true;
            labelFilename.Refresh();
        }

        void IProgress.ReportPercentage(double percentage)
        {
            progressBar1.Value = (int)Math.Round(percentage);
            progressBar1.Refresh();
        }

        void IProgress.ReportSomeProgress()
        {            
        }

        void IProgress.Start()
        {
            progressBar1.Visible = true;
            progressBar1.Style = ProgressBarStyle.Blocks;
            progressBar1.Value = 1;
            progressBar1.Refresh();

            labelFilename.Visible = false;
        }
    }
}
