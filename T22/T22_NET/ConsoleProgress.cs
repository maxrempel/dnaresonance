using System;

namespace Protocodes
{
    public class ConsoleProgress : IProgress
    {
        int currentLine = 0;

        public void End()
        {
            ReportPercentage(100);
            Console.SetCursorPosition(0, ++currentLine);
            Console.WriteLine("DONE");
        }

        public void ReportFile(string filename)
        {
            Console.SetCursorPosition(0, ++currentLine);
            Console.WriteLine($"File {filename}");
            ++currentLine;
        }

        public void WithFolder(string folder)
        {
            Console.SetCursorPosition(0, 0);
            Console.Write($"Folder {folder}");
        }

        public void ReportPercentage(double percentage)
        {
            Console.SetCursorPosition(0, currentLine);

            Console.Write($"- processed {percentage:00.0}%");
        }

        public void ReportSomeProgress()
        {
        }

        public void Start()
        {
            Console.Clear();
        }
    }
}
