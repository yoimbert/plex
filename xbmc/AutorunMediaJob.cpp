/*
 *      Copyright (C) 2005-2009 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
#include "AutorunMediaJob.h"
#include "utils/Builtins.h"
#include "GUIWindowManager.h"
#include "GUIDialogSelect.h"
#include "Key.h"

CAutorunMediaJob::CAutorunMediaJob(const CStdString &path)
{
  m_path = path;
}

void CAutorunMediaJob::DoWork()
{
  CGUIDialogSelect* pDialog= (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);

  pDialog->Reset();
  pDialog->SetHeading("New media detected");

  pDialog->Add("Browse videos");
  pDialog->Add("Browse music");
  pDialog->Add("Browse pictures");
  pDialog->Add("Browse files");

  pDialog->DoModal();

  int selection = pDialog->GetSelectedLabel();
  if (selection >= 0)
  {
    CStdString strAction;
    strAction.Format("ActivateWindow(%s, %s)", GetWindowString(selection), m_path.c_str());
    CBuiltins::Execute(strAction);
  }
}

const char *CAutorunMediaJob::GetWindowString(int selection)
{
  switch (selection)
  {
    case 0:
      return "VideoFiles";
    case 1:
      return "MusicFiles";
    case 2:
      return "Pictures";
    case 3:
      return "FileManager";
  }
}
