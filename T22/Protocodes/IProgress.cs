using System;
using System.Collections.Generic;
using System.Text;

namespace Protocodes
{
    public interface IProgress
    {
        void Start();
        void End();
        void ReportSomeProgress();

        // Operations
        void ReportFile(string filename);
        void ReportPercentage(double percentage);
    }
}
