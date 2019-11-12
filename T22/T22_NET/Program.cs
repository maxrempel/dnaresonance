using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using Protocodes;
using Serilog;

namespace T22
{
    class Program
    {
        static void Main(string[] args)
        {
            IProgress progress = null;
            if (args.Contains("--silent"))
            {
                progress = new SilentProgress();
            }
            else
            {
                progress = new ConsoleProgress();
            }

            string inputFolder = Directory.GetCurrentDirectory();
            int i = args.ToList().IndexOf("--folder");
            if (i >= 0 && i + 1 < args.Count())
            {
                inputFolder = args[i + 1];
            }

            string logfilepath = Path.Combine(inputFolder, "Log", "log-.txt");
            Log.Logger = new LoggerConfiguration()
                .WriteTo.File(logfilepath, rollingInterval: RollingInterval.Day)
                .CreateLogger();

            Stopwatch sw = new Stopwatch();
            sw.Start();

            try
            {
                new TopStopWork(progress)
                    .WithFolder(inputFolder)
                    .ProcessFolder();
            }
            catch (Exception ex)
            {
                Log.Error($"Error processing folder {inputFolder}: {ex}");
            }

            sw.Stop();
            Log.Information($"Folder {inputFolder} took {HumanTime(sw)} (more precisely, {sw.ElapsedMilliseconds / 1000.0} sec) to process.");
        }

        static string HumanTime(Stopwatch sw)
        {
            int hours = (int)Math.Floor(sw.Elapsed.TotalHours);
            var elapsed = sw.Elapsed - TimeSpan.FromHours(hours);
            int minutes = (int)Math.Floor(elapsed.TotalMinutes);
            elapsed -= TimeSpan.FromMinutes(minutes);
            int seconds = (int)Math.Floor(elapsed.TotalSeconds + 0.5); // rounded
            var humanTime = new StringBuilder();
            if (hours > 0) { humanTime.Append($"{hours} hours "); }
            if (minutes > 0) { humanTime.Append($"{minutes} minutes "); }
            humanTime.Append($"{seconds} seconds");
            // However
            if (hours == 0 && minutes == 0 && seconds == 0)
            {
                humanTime = new StringBuilder("less than a second");
            }
            return humanTime.ToString();
        }

    }
}
