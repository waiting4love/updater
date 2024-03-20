using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Demo2
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
            var w = UpdateService.VersionMessageWin_Create(Handle, true);
            //UpdateService.VersionMessageWin_SetShowingHandler(w, UpdateService.LABEL_TEXT_ALLCASE, IntPtr.Zero);
            //UpdateService.VersionMessageWin_EnableShowBoxOnClick(w, false, IntPtr.Zero, IntPtr.Zero);
            UpdateService.VersionMessageWin_EnablePerformUpdateOnExit(w, true);
            //var rect = ClientRectangle;
            //UpdateService.Rect rc = new UpdateService.Rect
            //{
            //    Left = 0,
            //    Top = 0,
            //    Right = rect.Right,
            //    Bottom = rect.Bottom
            //};
            //var lbl = UpdateService.VersionMessageLabel_Create(Handle, ref rc, 0, true);
            //UpdateService.VersionMessageLabel_SetAnchor(lbl, UpdateService.Anchor.Bottom | UpdateService.Anchor.Right);
            //UpdateService.VersionMessageLabel_EnableShowBoxOnClick(lbl, true, UpdateService.EXIT_BY_MESSAGE, IntPtr.Zero);
            //UpdateService.VersionMessageLabel_EnableAutoSize(lbl, true);
            //UpdateService.VersionMessageLabel_SetColor(lbl, new UpdateService.COLORREF(Color.DarkBlue));
            //UpdateService.VersionMessageLabel_EnablePerformUpdateOnExit(lbl, true);
        }

    }
}
