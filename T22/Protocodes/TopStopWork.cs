using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Protocodes
{
    public class TopStopWork
    {
        // Config
        IProgress progress = null;
        Dictionary<string, string> config = new Dictionary<string, string>();

        string inputFolder = Directory.GetCurrentDirectory();
        string outputFolder;

        // Input
        string FILENAME_CONFIG = "_T22_code.cfg";
        // Output
        string FILENAME_BONDS_TEMPLATE = "z_{filename}_BONDS.fa";
        string FILENAME_TOPSTOP_TEMPLATE = "z_{filename}_TOPSTOP.fa";
        string FILENAME_CHAINS_TEMPLATE = "z_{filename}_CHAINS.fa";
        string FILENAME_BTYPE_TEMPLATE = "z_{filename}_BTYPE.fa";
        string FILENAME_HISTOGRAM_TEMPLATE = "z_{filename}_ELENHIS.tab";

        Dictionary<string, char> Dinuc2BType = new Dictionary<string, char>();
        Dictionary<string, BOND> Dinuc2Bond = new Dictionary<string, BOND>();

        public TopStopWork()
        {
            Construct(new SilentProgress());
        }

        public TopStopWork(IProgress progressImpl)
        {
            Construct(progressImpl);
        }

        private void Construct(IProgress progressImpl)
        {
            progress = progressImpl;
            ConfigLog();
        }

        private void ConfigLog()
        {
            // No special configuration is required.
            // Configured in the calling code, such as T22.
        }

        public TopStopWork WithFolder(string folder)
        {
            inputFolder = folder;
            ConfigLog();
            return this;
        }

        public bool ProcessFolder()
        {
            progress.Start();

            try
            {
                ReadConfig();
            }
            catch (Exception ex)
            {
                Log.Error($"***Could not read config file: {ex.Message}");
                return false;
            }

            var filelist = GetInputFileList();
            foreach (var f in filelist)
            {
                progress.ReportFile(f);

                try
                {
                    ProcessFile(f);
                }
                catch (Exception ex)
                {
                    var msg = $"***Error processing file {f}: {ex.Message}";
                    Log.Error(msg);
                }
            }

            progress.ReportFile("Generating Elensum...");
            GenerateElensum();

            progress.End();
            return true;
        }

        void GenerateElensum()
        {
            string label_length_low_cutoff = "length_low_cutoff";

            int length_low_cutoff;
            if (!config.ContainsKey(label_length_low_cutoff))
            {
                length_low_cutoff = 0;
            }
            else if (!int.TryParse(config[label_length_low_cutoff], out length_low_cutoff))
            {
                Log.Warning($"Cannot generate Elensum file because length_low_cutoff is not defined");
                return;
            }

            string ELENHIS_SUFFIX = "_ELENHIS";
            string ELENHIS_PREFIX = "z_";

            var elenhisFiles = Directory.EnumerateFiles(outputFolder)
                .Where(f => new string[] {
                    ".tab"
                }.Contains(Path.GetExtension(f).ToLower()));
            elenhisFiles = elenhisFiles.Where(f => Path
                .GetFileNameWithoutExtension(f).ToLower()
                .EndsWith(ELENHIS_SUFFIX.ToLower()));

            string ELENSUM_TEMPLATE = "z_{folder}_ELENSUM.tab";
            string folder = Path.GetFileName(inputFolder);
            string elensumFilename = ELENSUM_TEMPLATE.Replace("{folder}", folder);
            string elensumFilepath = Path.Combine(outputFolder, elensumFilename);
            using (var outputStream = new StreamWriter(elensumFilepath))
            {
                // Header
                outputStream.WriteLine($">filename	total chain length with at least {length_low_cutoff}");

                foreach (var inputFilepath in elenhisFiles)
                {
                    using (var inputStream = new StreamReader(inputFilepath))
                    {
                        long theSum = 0;

                        for (var inputLine = inputStream.ReadLine(); inputLine != null; inputLine = inputStream.ReadLine())
                        {
                            // Skip header
                            if (inputLine[0] == '>')
                            {
                                continue;
                            }

                            var numbers = inputLine.Split(' ', '\t');
                            if (numbers.Length != 2)
                            {
                                Log.Warning($"GenerateElensum could not parse line \"{inputLine}\" in file {inputFilepath}");
                                continue;
                            }

                            int elementLength, lengthCount;
                            if (!int.TryParse(numbers[0], out elementLength))
                            {
                                Log.Warning(
                                    $"GenerateElensum: {numbers[0]} is not a valid element length in line \"{inputLine}\" in file {inputFilepath}");
                                continue;
                            }
                            if (!int.TryParse(numbers[1], out lengthCount))
                            {
                                Log.Warning(
                                    $"GenerateElensum: {numbers[1]} is not a valid length count in line \"{inputLine}\" in file {inputFilepath}");
                                continue;
                            }

                            if (elementLength < length_low_cutoff)
                            {
                                continue;
                            }

                            theSum += lengthCount;
                        }

                        string basename = Path.GetFileNameWithoutExtension(inputFilepath);
                        basename = basename.Substring(ELENHIS_PREFIX.Length);
                        basename = basename.Substring(0, basename.Length - ELENHIS_SUFFIX.Length);

                        outputStream.WriteLine($"{basename} {theSum}");
                    } // using inputStream
                } // foreach inputFilepath
            } // using outputStream

        } // GenerateElensum()

        void ReadConfig()
        {
            string configPath = Path.Combine(inputFolder, FILENAME_CONFIG);
            using (var f = new StreamReader(configPath))
            {
                string line;
                var separators = new char[] { ' ', '\t' };

                for (int linecount = 0; (line = f.ReadLine()) != null; ++linecount)
                {
                    // Skip empty strings.
                    line = line.Trim();
                    if (line.Length < 1)
                    {
                        continue;
                    }

                    // Comment?
                    var initial = line[0];
                    if (initial == '>') { continue; }

                    var words = line.Split(separators);

                    // Dinuc.
                    var dinuc = words[0];
                    if (dinuc.Length != 2)
                    {
                        // Assume it is a parameter setting in the format
                        // parameter=value
                        words = dinuc.Split(new char[] { '=' });
                        if (words.Length != 2)
                        {
                            var msg = $"***Config error at line {linecount}: \"{line}\" does not parse.";
                            Log.Error(msg);
                            throw new Exception(msg);
                        }
                        config[words[0]] = words[1];
                        continue;
                    }
                    if (Dinuc2BType.ContainsKey(dinuc))
                    {
                        Log.Warning($"Config at line {linecount}: dinuc {dinuc} has already been configured." +
                            " Using the initial configuration.");
                        continue;
                    }

                    // B type.
                    var btype = words[1].ToUpper();
                    char bt = btype.ToCharArray()[0];
                    Dinuc2BType[dinuc] = bt;

                    // Bond.
                    var bondDescription = words[2]
                        .Replace('0', 'O').Replace('|', 'I')
                        .Replace("^", "_TOP").Replace("v", "_BOTTOM")
                        .Replace("=", "_BOTH").Replace(":", "_NONE");
                    BOND bond;
                    if (Enum.TryParse<BOND>(bondDescription, true, out bond))
                    {
                        Dinuc2Bond[dinuc] = bond;
                    }
                    else
                    {
                        throw new Exception($"Could not understand Bond {words[2]}");
                    }
                }
            }
        } // ReadConfig()

        /// <summary>
        /// List all files in the input folder which contain input data.
        /// </summary>
        /// <returns></returns>
        IEnumerable<string> GetInputFileList()
        {
            return Directory.EnumerateFiles(inputFolder)
                .Where(f => new string[] {
                    ".fa", ".fasta"
                }.Contains(Path.GetExtension(f).ToLower()))
                ;
        }

        /// <summary>
        /// Main logic.
        /// Also reports progress (estimated percentage).
        /// </summary>
        /// <param name="inputFilepath"></param>
        void ProcessFile(string inputFilepath)
        {
            FileInfo fi = new FileInfo(inputFilepath);
            long percent80 = fi.Length;
            long percentCurrent = 0;

            // Input.
            const int bufferLength = 4000;
            var buffer = new char[bufferLength];

            // Input and output.
            string inputHeader = "";

            // Output.
            outputFolder = Path.Combine(inputFolder, "OUTPUT");
            if (!Directory.Exists(outputFolder))
            {
                Directory.CreateDirectory(outputFolder);
            }

            /// The number of elements in the topstop sequence
            /// (also the number of bonds, the number of btype codes).
            long topstopLength;

            string basename = Path.GetFileNameWithoutExtension(inputFilepath);

            string bondsFilename = FILENAME_BONDS_TEMPLATE.Replace("{filename}", basename);
            string bondsFilepath = Path.Combine(outputFolder, bondsFilename);

            string btypeFilename = FILENAME_BTYPE_TEMPLATE.Replace("{filename}", basename);
            string btypeFilepath = Path.Combine(outputFolder, btypeFilename);

            // Circuits.
            // Snapshots of the current positions of each circuit.
            CIRCUIT_POSITION circuit1 = CIRCUIT_POSITION.NONE;
            CIRCUIT_POSITION circuit2 = CIRCUIT_POSITION.NONE;
            // (Latest) position at which each circuit started.
            long circuit1_start = -1;
            long circuit2_start = -1;

            // Start/end positions of the circuits.
            // Inclusive on the left, exclusive on the right.
            var start1 = new List<long>();
            var end1 = new List<long>();
            var start2 = new List<long>();
            var end2 = new List<long>();

            // FIRST PASS

            using (var f = new StreamReader(inputFilepath))
            {
                // Calculate the start/end positions.
                // Generate the bonds output file.
                // Generate the btype output file.

                using (var b = new StreamWriter(bondsFilepath))
                {
                    using (var t = new StreamWriter(btypeFilepath))
                    {
                        // LAMBDAS

                        Action<long> finalizeCircuit1 = (pos) =>
                        {
                            if (circuit1_start >= 0)
                            {
                                // start/end
                                start1.Add(circuit1_start);
                                end1.Add(pos);
                                // ongoing
                                circuit1_start = -1;
                                circuit1 = CIRCUIT_POSITION.NONE;
                            }
                        };
                        Action<long> finalizeCircuit2 = (pos) =>
                        {
                            if (circuit2_start >= 0)
                            {
                                // start/end
                                start2.Add(circuit2_start);
                                end2.Add(pos);
                                // ongoing
                                circuit2_start = -1;
                                circuit2 = CIRCUIT_POSITION.NONE;
                            }
                        };
                        Action<long> finalizeCircuits = (pos) =>
                        {
                            finalizeCircuit1(pos);
                            finalizeCircuit2(pos);
                        };

                        Action<BOND, long> processBond = (bond, pos) =>
                        {
                            if (bond.HasOrthogonalBond())
                            // The two (what seemed to be parallel) circuits may be connected here,
                            // or a single circuit could make a bend.
                            // The latter case is delayed until we consider the coaxial bonds.
                            {
                                if (
                                    (circuit1 == CIRCUIT_POSITION.TOP && circuit2 == CIRCUIT_POSITION.BOTTOM)
                                    || (circuit1 == CIRCUIT_POSITION.BOTTOM && circuit2 == CIRCUIT_POSITION.TOP))
                                {
                                    // Both circuits have been parallel for a while.
                                    // Connect them: the earler circuit wins.
                                    if (circuit1_start <= circuit2_start)
                                    {
                                        circuit1 = CIRCUIT_POSITION.BOTH;
                                        circuit2 = CIRCUIT_POSITION.NONE;
                                        circuit2_start = -1;
                                    }
                                    else
                                    {
                                        circuit2 = CIRCUIT_POSITION.BOTH;
                                        circuit1 = CIRCUIT_POSITION.NONE;
                                        circuit1_start = -1;
                                    }
                                }
                            }

                            if (!bond.HasTopCoaxialBond() && !bond.HasBottomCoaxialBond())
                            // No coaxial bond: a break.
                            {
                                finalizeCircuits(pos);
                                return;
                            }

                            // Has one or both coaxial bonds.
                            if (!bond.HasBottomCoaxialBond())
                            // Only the top coaxial bond.
                            {
                                if (circuit1 == CIRCUIT_POSITION.NONE && circuit2 == CIRCUIT_POSITION.NONE)
                                {
                                    circuit1 = CIRCUIT_POSITION.TOP;
                                    circuit1_start = pos;
                                    return;
                                }

                                if (circuit1 == CIRCUIT_POSITION.BOTH)
                                {
                                    circuit1 = CIRCUIT_POSITION.TOP;
                                    return;
                                }
                                if (circuit2 == CIRCUIT_POSITION.BOTH)
                                {
                                    circuit2 = CIRCUIT_POSITION.TOP;
                                    return;
                                }

                                if (circuit1 == CIRCUIT_POSITION.BOTTOM)
                                {
                                    if (bond.HasOrthogonalBond())
                                    {
                                        circuit1 = CIRCUIT_POSITION.TOP;
                                        return;
                                    }
                                    finalizeCircuit1(pos);
                                    if (circuit2 == CIRCUIT_POSITION.NONE)
                                    {
                                        circuit2 = CIRCUIT_POSITION.TOP;
                                        circuit2_start = pos;
                                        return;
                                    }
                                }
                                if (circuit2 == CIRCUIT_POSITION.BOTTOM)
                                {
                                    if (bond.HasOrthogonalBond())
                                    {
                                        circuit2 = CIRCUIT_POSITION.TOP;
                                        return;
                                    }
                                    finalizeCircuit2(pos);
                                    if (circuit1 == CIRCUIT_POSITION.NONE)
                                    {
                                        circuit1 = CIRCUIT_POSITION.TOP;
                                        circuit1_start = pos;
                                        return;
                                    }
                                }

                                return;
                            }

                            if (!bond.HasTopCoaxialBond())
                            // Only the bottom coaxial bond.
                            {
                                if (circuit1 == CIRCUIT_POSITION.NONE && circuit2 == CIRCUIT_POSITION.NONE)
                                {
                                    circuit1 = CIRCUIT_POSITION.BOTTOM;
                                    circuit1_start = pos;
                                    return;
                                }

                                if (circuit1 == CIRCUIT_POSITION.BOTH)
                                {
                                    circuit1 = CIRCUIT_POSITION.BOTTOM;
                                    return;
                                }
                                if (circuit2 == CIRCUIT_POSITION.BOTH)
                                {
                                    circuit2 = CIRCUIT_POSITION.BOTTOM;
                                    return;
                                }

                                if (circuit1 == CIRCUIT_POSITION.TOP)
                                {
                                    if (bond.HasOrthogonalBond())
                                    {
                                        circuit1 = CIRCUIT_POSITION.BOTTOM;
                                        return;
                                    }
                                    finalizeCircuit1(pos);
                                    if (circuit2 == CIRCUIT_POSITION.NONE)
                                    {
                                        circuit2 = CIRCUIT_POSITION.BOTTOM;
                                        circuit2_start = pos;
                                        return;
                                    }
                                }
                                if (circuit2 == CIRCUIT_POSITION.TOP)
                                {
                                    if (bond.HasOrthogonalBond())
                                    {
                                        circuit2 = CIRCUIT_POSITION.BOTTOM;
                                        return;
                                    }
                                    finalizeCircuit2(pos);
                                    if (circuit1 == CIRCUIT_POSITION.NONE)
                                    {
                                        circuit1 = CIRCUIT_POSITION.BOTTOM;
                                        circuit1_start = pos;
                                        return;
                                    }
                                }

                                return;
                            }

                            // Both coaxial bonds.
                            if (circuit1 == CIRCUIT_POSITION.BOTH || circuit2 == CIRCUIT_POSITION.BOTH)
                            {
                                return;
                            }

                            if (circuit1 == CIRCUIT_POSITION.NONE && circuit2 == CIRCUIT_POSITION.NONE)
                            {
                                if (bond.HasOrthogonalBond())
                                {
                                    circuit1 = CIRCUIT_POSITION.BOTH;
                                    circuit1_start = pos;
                                    return;
                                }
                                circuit1 = CIRCUIT_POSITION.TOP;
                                circuit1_start = pos;
                                circuit2 = CIRCUIT_POSITION.BOTTOM;
                                circuit2_start = pos;
                                return;
                            }

                            if (circuit1 == CIRCUIT_POSITION.TOP)
                            {
                                if (bond.HasOrthogonalBond())
                                {
                                    circuit1 = CIRCUIT_POSITION.BOTH;
                                    return;
                                }
                                // Otherwise no changes to circuit1.

                                if (circuit2 == CIRCUIT_POSITION.NONE)
                                {
                                    circuit2 = CIRCUIT_POSITION.BOTTOM;
                                    circuit2_start = pos;
                                }
                                // Otherwise no changes to circuit2.

                                return;
                            }

                            if (circuit1 == CIRCUIT_POSITION.BOTTOM)
                            {
                                if (bond.HasOrthogonalBond())
                                {
                                    circuit1 = CIRCUIT_POSITION.BOTH;
                                    return;
                                }
                                // Otherwise no changes to circuit1.

                                if (circuit2 == CIRCUIT_POSITION.NONE)
                                {
                                    circuit2 = CIRCUIT_POSITION.TOP;
                                    circuit2_start = pos;
                                }
                                // Otherwise no changes to circuit2.

                                return;
                            }

                            // circuit1 is NONE, circuit2 exists.
                            if (bond.HasOrthogonalBond())
                            {
                                circuit2 = CIRCUIT_POSITION.BOTH;
                                return;
                            }
                            // circuit1 is NONE, circuit2 is either TOP or BOTTOM.
                            if (circuit2 == CIRCUIT_POSITION.TOP)
                            {
                                circuit1 = CIRCUIT_POSITION.BOTTOM;
                                circuit1_start = pos;
                                return;
                            }
                            circuit1 = CIRCUIT_POSITION.TOP;
                            circuit1_start = pos;
                            return;
                        };

                        // WORK

                        // Prefill.
                        f.ReadBlock(buffer, bufferLength - 1, 1);
                        // Skip the header, if any.
                        while (buffer[bufferLength - 1] == '>')
                        {
                            inputHeader += ">" + f.ReadLine();
                            f.ReadBlock(buffer, bufferLength - 1, 1);
                        }
                        // Skip control characters, if any.
                        while (char.IsControl(buffer[bufferLength - 1]))
                        {
                            f.ReadBlock(buffer, bufferLength - 1, 1);
                        }

                        // Header
                        if (!string.IsNullOrWhiteSpace(inputHeader))
                        {
                            b.WriteLine(inputHeader);
                            t.WriteLine(inputHeader);
                        }

                        long position = 0;

                        // Iterate...
                        int howMuchRead, howMuchExpected;
                        do
                        {
                            buffer[0] = buffer[bufferLength - 1];
                            howMuchExpected = bufferLength - 1;
                            howMuchRead = f.ReadBlock(buffer, 1, howMuchExpected);
                            // Now buffer contains the next portion of the input
                            // in elements 0 through howMuchRead inclusive.

                            StringBuilder sb = new StringBuilder("  ");
                            sb[1] = buffer[0];

                            string dinuc;

                            // Process dinucs starting at index 0 to howMuchRead inclusively.
                            for (int index = 1; index <= howMuchRead; index++, position++)
                            {
                                if (char.IsControl(buffer[index]))
                                {
                                    if (index > 0)
                                    {
                                        buffer[index] = buffer[index - 1]; // to keep the dinuc together
                                    }
                                    --position; // undo increment
                                    continue;
                                }

                                sb[0] = sb[1];
                                sb[1] = buffer[index];

                                dinuc = sb.ToString().ToLower();

                                if (Dinuc2Bond.ContainsKey(dinuc))
                                {
                                    try
                                    {
                                        var bond = Dinuc2Bond[dinuc];
                                        var bt = Dinuc2BType[dinuc];
                                        // Output files.
                                        b.Write(bond.ToPrint());
                                        t.Write(bt.ToString().ToLower());
                                        // Circuits.
                                        processBond(bond, position);
                                    }
                                    catch (Exception ex)
                                    {
                                        var msg = $"Internal error: index={index}, dinuc={dinuc}, error message is {ex.Message}";
                                        Log.Error(msg);
                                        throw new Exception(msg);
                                    }
                                }
                                else
                                {
                                    // Bonds output file.
                                    b.Write(".x");
                                    t.Write('-');
                                    // Circuits.
                                    finalizeCircuits(position);
                                }

                            } // for index

                            percentCurrent += howMuchRead;
                            progress.ReportPercentage((double)percentCurrent * 80 / percent80);

                        } while (howMuchRead >= howMuchExpected);

                        finalizeCircuits(position);

                        topstopLength = position;
                    } // using t
                } // using b

            } // using f

            // SECOND PHASE
            // (no pass over the input).
            // Depends on the first phase.
            // Calculate the histogram,
            // generate the histogram output file.
            var Length2Count = new Dictionary<long, long>();
            for (int i = 0; i < start1.Count; i++)
            {
                var len = end1[i] - start1[i];
                if (Length2Count.ContainsKey(len))
                {
                    ++Length2Count[len];
                }
                else
                {
                    Length2Count[len] = 1;
                }
            }

            progress.ReportPercentage(81);

            for (int i = 0; i < start2.Count; i++)
            {
                var len = end2[i] - start2[i];
                if (Length2Count.ContainsKey(len))
                {
                    ++Length2Count[len];
                }
                else
                {
                    Length2Count[len] = 1;
                }
            }

            progress.ReportPercentage(82);

            string histogramFilename = FILENAME_HISTOGRAM_TEMPLATE.Replace("{filename}", basename);
            string histogramFilepath = Path.Combine(outputFolder, histogramFilename);

            using (var h = new StreamWriter(histogramFilepath))
            {
                // Header
                h.WriteLine("> ELEN; count");

                // Body
                foreach (var kvp in Length2Count.OrderBy(l2c => l2c.Key))
                {
                    h.WriteLine($"{kvp.Key}\t{kvp.Value}");
                }
            }

            progress.ReportPercentage(83);

            // THIRD PHASE
            // (no pass over the input file).
            // Depends on the first phase:
            // scans the positions and accounts for start/end of the circuits.
            // Generates the topstop output file.

            string topstopFilename = FILENAME_TOPSTOP_TEMPLATE.Replace("{filename}", basename);
            string topstopFilepath = Path.Combine(outputFolder, topstopFilename);

            string chainsFilename = FILENAME_CHAINS_TEMPLATE.Replace("{filename}", basename);
            string chainsFilepath = Path.Combine(outputFolder, chainsFilename);

            using (var t = new StreamWriter(topstopFilepath))
            {
                // Header
                if (!string.IsNullOrWhiteSpace(inputHeader))
                {
                    t.WriteLine(inputHeader);
                }

                // Body

                // Calculate the topstop sequence
                // according to the ipyx (overlap/overlay/crossout) definition.

                PrintTopstopAndShiftPosition g = new PrintTopstopAndShiftPosition(t);

                int i1 = 0, i2 = 0; // index into start1/end1 and start2/end2 respectively

                double percentAccumulated = 83;
                double percentRemaining = 100 - percentAccumulated;
                long percentReportEach = (long)(topstopLength / percentRemaining / 10.0);
                if (percentReportEach == 0) { percentReportEach = 1; }

                // Scans through the positions. Prints the status at that position.
                // For each value of pos, i1 is the first segment from the first list that we have not gone past
                // (that is, end1[i1] >= pos), unless i1 is out of range (then the first list has been exhausted).
                // We may be before that segment or inside it.
                // Similarly, i2 and the second list.
                for (long pos = 0; pos < topstopLength; ++pos)
                {
                    if (pos % percentReportEach == 0)
                    {
                        var percentAdded = percentRemaining * pos / topstopLength;
                        progress.ReportPercentage(percentAccumulated + percentAdded);
                    }

                    // X?
                    // .1
                    // 2.
                    if (i2 < end2.Count && i1 < start1.Count && end2[i2] == start1[i1] && end2[i2] == pos)
                    {
                        g.Go(TOPSTOP.X);
                        ++i2;
                        continue;
                    }
                    if (i2 < end2.Count && i1 < start1.Count && end2[i2] == start1[i1] && end2[i2] == pos + 1)
                    {
                        g.Go(TOPSTOP.X);
                        continue;
                    }
                    if (i2 < end2.Count && i1 + 1 < start1.Count && end2[i2] == start1[i1 + 1] && end2[i2] == pos + 1)
                    {
                        g.Go(TOPSTOP.X);
                        if (end1[i1] == pos)
                        {
                            ++i1;
                        }
                        continue;
                    }
                    // 1.
                    // .2
                    if (i1 < end1.Count && i2 < start2.Count && end1[i1] == start2[i2] && end1[i1] == pos)
                    {
                        g.Go(TOPSTOP.X);
                        ++i1;
                        continue;
                    }
                    if (i1 < end1.Count && i2 < start2.Count && end1[i1] == start2[i2] && end1[i1] == pos + 1)
                    {
                        g.Go(TOPSTOP.X);
                        continue;
                    }
                    if (i1 < end1.Count && i2 + 1 < start2.Count && end1[i1] == start2[i2 + 1] && end1[i1] == pos + 1)
                    {
                        g.Go(TOPSTOP.X);
                        if (end2[i2] == pos)
                        {
                            ++i2;
                        }
                        continue;
                    }

                    // Which circuits (start/end lists) still exist?
                    if (i1 >= end1.Count && i2 >= end2.Count)
                    {
                        // Both circuits ended.
                        g.Go(TOPSTOP.S, topstopLength);
                        pos = topstopLength;
                        continue; // actually, finish
                    }
                    // At least one circuit exists.

                    if (i1 >= end1.Count)
                    {
                        // Circuit 1 ended.
                        if (start2[i2] <= pos && pos < end2[i2])
                        {
                            g.Go(TOPSTOP.I);
                            continue;
                        }
                        if (end2[i2] == pos)
                        {
                            g.Go(TOPSTOP.S);
                            ++i2;
                            continue;
                        }
                        g.Go(TOPSTOP.S);
                        continue;
                    }

                    if (i2 >= end2.Count)
                    {
                        // Circuit 2 ended.
                        if (start1[i1] <= pos && pos < end1[i1])
                        {
                            g.Go(TOPSTOP.I);
                            continue;
                        }
                        if (end1[i1] == pos)
                        {
                            g.Go(TOPSTOP.S);
                            ++i1;
                            continue;
                        }
                        g.Go(TOPSTOP.S);
                        continue;
                    }

                    // Both circuits still exist.
                    
                    // Consider all cases (long if), then check if any segments end here.
                    if (start1[i1] <= pos && pos < end1[i1] && start2[i2] <= pos && pos < end2[i2])
                    {
                        // Both circuits at this position.
                        // Can be an overlap or overlay.
                        if (start1[i1] <= start2[i2] && end1[i1] >= end2[i2])
                        {
                            // Overlay
                            g.Go(TOPSTOP.Y);
                        }
                        else if (start2[i2] <= start1[i1] && end2[i2] >= end1[i1])
                        {
                            // Overlay
                            g.Go(TOPSTOP.Y);
                        }
                        else if (start1[i1] <= start2[i2] && end1[i1] < end2[i2])
                        {
                            // Overlap
                            g.Go(TOPSTOP.P);
                        }
                        else if (start2[i2] <= start1[i1] && end2[i2] < end1[i1])
                        {
                            // Overlap
                            g.Go(TOPSTOP.P);
                        }
                    }

                    // No more than one circuit at this position.

                    else if (
                        (start1[i1] <= pos && pos < end1[i1]) ||
                        (start2[i2] <= pos && pos < end2[i2]))
                    {
                        // Exactly one circuit at this position.
                        g.Go(TOPSTOP.I);
                    }
                    else
                    {
                        // No circuits at this position.
                        g.Go(TOPSTOP.S);
                    }

                    // End of a segment?
                    if (end1[i1] == pos)
                    {
                        ++i1;
                    }
                    if (end2[i2] == pos)
                    {
                        ++i2;
                    }

                } // for pos

            } // using t

            // FOURTH PHASE
            // Create another output file, CHAINS, based on the topstop file, 
            // but with a different set of letters.
            GenerateChainsFileFromTopstopFile(topstopFilepath);

            progress.ReportPercentage(100);
        } // ProcessFile()

        /// <summary>
        /// Chains format is similar to ipyx topstop format
        /// except the characters are transformed as in: <br/><br/>tr 'ipyxs' 'acgtn'
        /// </summary>
        /// <param name="topstopFilepath"></param>
        public void GenerateChainsFileFromTopstopFile(string topstopFilepath)
        {
            // Folder and file.
            string chainsFilepath = topstopFilepath.Replace("_TOPSTOP.", "_CHAINS.");

            // Translation table.
            var table = new Dictionary<char, char>();
            table.Add('i', 'a');
            table.Add('p', 'c');
            table.Add('y', 'g');
            table.Add('x', 't');
            table.Add('s', 'n');

            using (var tr = new StreamReader(topstopFilepath))
            {
                using (var c = new StreamWriter(chainsFilepath))
                {
                    // Header, if any
                    string inputHeader = "";
                    var peeked = (char)tr.Peek();
                    if ('>' == peeked)
                    {
                        inputHeader = tr.ReadLine();
                    }

                    if (!string.IsNullOrWhiteSpace(inputHeader))
                    {
                        c.WriteLine(inputHeader);
                    }

                    // Body
                    const int bufferLength = 8192;
                    var buffer = new char[bufferLength];

                    int hasRead = 0;
                    do
                    {
                        hasRead = tr.ReadBlock(buffer, 0, bufferLength);
                        for (int i = 0; i < hasRead; ++i)
                        {
                            char ch = buffer[i];
                            if (table.ContainsKey(ch))
                            {
                                c.Write(table[ch]);
                            }
                        }
                    } while (hasRead == bufferLength);

                } // using c
            } // using tr

        } // GenerateChainsFileFromTopstopFile()

    }
}
