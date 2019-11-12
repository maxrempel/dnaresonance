namespace Protocodes
{
    static class Extensions
    {
        #region BOND

        public static bool HasOrthogonalBond(this BOND bond)
        {
            switch (bond)
            {
                case BOND.I_TOP:
                case BOND.I_BOTH:
                case BOND.I_BOTTOM:
                case BOND.I_NONE:
                    return true;
                default:
                    return false;
            }
        }

        public static bool HasTopCoaxialBond(this BOND bond)
        {
            switch (bond)
            {
                case BOND.I_TOP:
                case BOND.I_BOTH:
                case BOND.O_TOP:
                case BOND.O_BOTH:
                    return true;
                default:
                    return false;
            }
        }

        public static bool HasBottomCoaxialBond(this BOND bond)
        {
            switch (bond)
            {
                case BOND.I_BOTH:
                case BOND.I_BOTTOM:
                case BOND.O_BOTH:
                case BOND.O_BOTTOM:
                    return true;
                default:
                    return false;
            }
        }

        public static string ToPrint(this BOND bond)
        {
            switch (bond)
            {
                case BOND.I_TOP:
                    return "I^";
                case BOND.I_BOTH:
                    return "I=";
                case BOND.I_BOTTOM:
                    return "Iv";
                case BOND.I_NONE:
                    return "I:";
                case BOND.O_TOP:
                    return "0^";
                case BOND.O_BOTH:
                    return "0=";
                case BOND.O_BOTTOM:
                    return "0v";
                case BOND.O_NONE:
                    return "0:";
                default:
                    return ".x";
            }

        }

        #endregion

        #region TOPSTOP

        public static char ToChar(this TOPSTOP ts)
        {
            switch (ts)
            {
                case TOPSTOP.S:
                    return 's';
                case TOPSTOP.I:
                    return 'i';
                case TOPSTOP.P:
                    return 'p';
                case TOPSTOP.Y:
                    return 'y';
                case TOPSTOP.X:
                    return 'x';
                default:
                    return '-';
            }
        }

        #endregion

    }
}
