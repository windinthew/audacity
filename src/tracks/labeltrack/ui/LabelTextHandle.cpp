/**********************************************************************

Audacity: A Digital Audio Editor

LabelTextHandle.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../Audacity.h"
#include "LabelTextHandle.h"
#include "../../../HitTestResult.h"
#include "../../../LabelTrack.h"
#include "../../../Project.h"
#include "../../../RefreshCode.h"
#include "../../../TrackPanelMouseEvent.h"
#include "../../../ViewInfo.h"
#include "../../../images/Cursors.h"

LabelTextHandle::LabelTextHandle()
{
}

LabelTextHandle &LabelTextHandle::Instance()
{
   static LabelTextHandle instance;
   return instance;
}

HitTestPreview LabelTextHandle::HitPreview()
{
   static auto ibeamCursor =
      ::MakeCursor(wxCURSOR_IBEAM, IBeamCursorXpm, 17, 16);
   return {
      _("Click to edit label text"),
      ibeamCursor.get()
   };
}

HitTestResult LabelTextHandle::HitTest
(const wxMouseEvent &event, const std::shared_ptr<LabelTrack> &pLT)
{
   // If Control is down, let the select handle be hit instead
   if (!event.ControlDown() &&
       pLT->OverATextBox(event.m_x, event.m_y) >= 0)
      // There was no cursor change or status message for mousing over a label text box
      return { HitPreview(), &Instance() };

   return {};
}

LabelTextHandle::~LabelTextHandle()
{
}

UIHandle::Result LabelTextHandle::Click
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   auto result = LabelDefaultClickHandle::Click( evt, pProject );

   auto &selectionState = pProject->GetSelectionState();
   TrackList *const tracks = pProject->GetTracks();
   mChanger =
      std::make_unique< SelectionStateChanger >( selectionState, *tracks );

   const auto pCell = evt.pCell;
   const wxMouseEvent &event = evt.event;
   ViewInfo &viewInfo = pProject->GetViewInfo();

   auto pLT = std::static_pointer_cast<LabelTrack>(pCell);
   mpLT = pLT;
   mSelectedRegion = viewInfo.selectedRegion;
   pLT->HandleTextClick( event, evt.rect, viewInfo, &viewInfo.selectedRegion );
   wxASSERT(pLT->IsSelected());

   {
      // IF the user clicked a label, THEN select all other tracks by Label

      TrackListIterator iter(tracks);
      Track *t = iter.First();

      //do nothing if at least one other track is selected
      bool done = false;
      while (!done && t) {
         if (t->GetSelected() && t != pLT.get())
            done = true;
         t = iter.Next();
      }

      if (!done) {
         //otherwise, select all tracks
         t = iter.First();
         while (t)
         {
            selectionState.SelectTrack
               ( *pProject->GetTracks(), *t, true, true,
                 pProject->GetMixerBoard() );
            t = iter.Next();
         }
      }

      // Do this after, for its effect on TrackPanel's memory of last selected
      // track (which affects shift-click actions)
      selectionState.SelectTrack
         ( *pProject->GetTracks(), *pLT, true, true,
           pProject->GetMixerBoard() );
   }

   // PRL: bug1659 -- make selection change undo correctly
   const bool unsafe = pProject->IsAudioActive();
   if (!unsafe)
      pProject->ModifyState(false);

   return result | RefreshCode::RefreshCell | RefreshCode::UpdateSelection;
}

UIHandle::Result LabelTextHandle::Drag
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   using namespace RefreshCode;
   auto result = LabelDefaultClickHandle::Drag( evt, pProject );

   const wxMouseEvent &event = evt.event;
   auto pLT = pProject->GetTracks()->Lock(mpLT);
   if(pLT)
      pLT->HandleTextDragRelease(event);

   // locate the initial mouse position
   if (event.LeftIsDown()) {
      if (mLabelTrackStartXPos == -1) {
         mLabelTrackStartXPos = event.m_x;
         mLabelTrackStartYPos = event.m_y;

         if (pLT &&
            (pLT->getSelectedIndex() != -1) &&
             pLT->OverTextBox(
               pLT->GetLabel(pLT->getSelectedIndex()),
               mLabelTrackStartXPos,
               mLabelTrackStartYPos))
            mLabelTrackStartYPos = -1;
      }
      // if initial mouse position in the text box
      // then only drag text
      if (mLabelTrackStartYPos == -1)
         result |= RefreshCell;
   }

   return result;
}

HitTestPreview LabelTextHandle::Preview
(const TrackPanelMouseEvent &evt, const AudacityProject *pProject)
{
   return HitPreview();
}

UIHandle::Result LabelTextHandle::Release
(const TrackPanelMouseEvent &evt, AudacityProject *pProject,
 wxWindow *pParent)
{
   auto result = LabelDefaultClickHandle::Release( evt, pProject, pParent );

   // Only selected a part of a text string and changed track selectedness.
   // No undoable effects.

   if (mChanger) {
      mChanger->Commit();
      mChanger.release();
   }

   const wxMouseEvent &event = evt.event;
   auto pLT = pProject->GetTracks()->Lock(mpLT);
   if (pLT)
      pLT->HandleTextDragRelease(event);

   // handle mouse left button up
   if (event.LeftUp())
      mLabelTrackStartXPos = -1;

   return result | RefreshCode::RefreshNone;
}

UIHandle::Result LabelTextHandle::Cancel( AudacityProject *pProject )
{
   // Restore the selection states of tracks
   // Note that we are also relying on LabelDefaultClickHandle::Cancel
   // to restore the selection state of the labels in the tracks.
   mChanger.release();
   ViewInfo &viewInfo = pProject->GetViewInfo();
   viewInfo.selectedRegion = mSelectedRegion;
   auto result = LabelDefaultClickHandle::Cancel( pProject );
   return result | RefreshCode::RefreshAll;
}
