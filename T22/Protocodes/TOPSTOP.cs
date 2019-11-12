namespace Protocodes
{
    enum TOPSTOP
    {
        /// <summary>
        /// Stop (no circuits)
        /// </summary>
        S,
        /// <summary>
        /// One strand, no crossout
        /// </summary>
        I,
        /// <summary>
        /// Overlap (the tails are in the opposite strands)
        /// </summary>
        P,
        /// <summary>
        /// Overlay ()
        /// </summary>
        Y,
        /// <summary>
        /// One strand with crossout (two chains coming together without contact)
        /// </summary>
        X,
    }
}
