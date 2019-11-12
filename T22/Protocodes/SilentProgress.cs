using System;
using System.Collections.Generic;
using System.Text;

namespace Protocodes
{
    public class SilentProgress : IProgress
    {
        public void End()
        {
        }

        public void ReportFile(string filename)
        {
        }

        public IProgress WithFolder(string folder)
        {
            return this;
        }

        public void ReportPercentage(double percentage)
        {
        }

        public void ReportSomeProgress()
        {            
        }

        public void Start()
        {
        }
    }
}
