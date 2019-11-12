using System.IO;

namespace Protocodes
{
    class PrintTopstopAndShiftPosition
    {
        public TOPSTOP Status { get; set; }
        public long Position { get; set; }
        public StreamWriter SW { get; set; }

        public PrintTopstopAndShiftPosition(StreamWriter sw, long position = 0)
        {
            Position = position;
            SW = sw;
        }

        /// <summary>
        /// Set the status.<br/>
        /// Output it for (toWhere - Position) positions
        /// (and then set Position to toWhere),
        /// unless that value is not positive.<br/>
        /// Return the new Position.
        /// </summary>
        /// <param name="status"></param>
        /// <param name="toWhere"></param>
        /// <returns>Position</returns>
        public long Go(TOPSTOP status, long toWhere)
        {
            Status = status;

            int howMuch = (int)(toWhere - Position);
            if (howMuch <= 0) { return Position; }

            var ch = status.ToChar();
            for (int i = 0; i < howMuch; i++)
            {
                SW.Write(ch);
            }
            return Position = toWhere;
        }

        /// <summary>
        /// Set the status.
        /// Print it and increment the position.
        /// </summary>
        /// <param name="status"></param>
        /// <returns></returns>
        public long Go(TOPSTOP status)
        {
            Status = status;

            SW.Write(status.ToChar());
            return ++Position;
        }

    }
}
