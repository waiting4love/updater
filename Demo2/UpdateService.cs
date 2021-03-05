using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace Demo2
{
    static public class UpdateService
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct Rect
        {
            public int Left;        // x position of upper-left corner
            public int Top;         // y position of upper-left corner
            public int Right;       // x position of lower-right corner
            public int Bottom;      // y position of lower-right corner
        }

        public enum Anchor : uint
        {
            None = 0,
            Left = 1,
            Top = 2,
            Right = 4,
            Bottom = 8
        }

        public struct COLORREF
        {
            private uint ColorDWORD;

            public COLORREF(System.Drawing.Color color)
            {
                ColorDWORD = (uint)color.R + (((uint)color.G) << 8) + (((uint)color.B) << 16);
            }

            public System.Drawing.Color GetColor()
            {
                return System.Drawing.Color.FromArgb((int)(0x000000FFU & ColorDWORD),
               (int)(0x0000FF00U & ColorDWORD) >> 8, (int)(0x00FF0000U & ColorDWORD) >> 16);
            }

            public void SetColor(System.Drawing.Color color)
            {
                ColorDWORD = (uint)color.R + (((uint)color.G) << 8) + (((uint)color.B) << 16);
            }
        }

        public static readonly IntPtr EXIT_BY_MESSAGE = new IntPtr(-1);
        public static readonly IntPtr EXIT_BY_DESTROYWINDOW = new IntPtr(-2);
        public static readonly IntPtr EXIT_BY_ENDDIALOG = new IntPtr(-3);

        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern IntPtr VersionMessageLabel_Create(IntPtr parent, ref Rect rect, uint id, bool manage_update_instance);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_EnableShowBoxOnClick(IntPtr label, bool enable, IntPtr request_exit, IntPtr param);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_EnablePerformUpdateOnExit(IntPtr label, bool enable);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_EnableAutoSize(IntPtr label, bool enable);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_SetAlignment(IntPtr label, bool RightAlign, bool BottomAlign);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_SetColor(IntPtr label, COLORREF color);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_SetFont(IntPtr label, int nPointSize, String lpszFaceName);
        [DllImport("UpdateService.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        internal static extern void VersionMessageLabel_SetAnchor(IntPtr label, Anchor anchor);
    }
}
