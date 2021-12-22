#include "ImSequencer.h"
#include <imgui_helper.h>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ImSequencer
{
std::string MillisecToString(int64_t millisec, bool show_millisec = false)
{
    std::ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    if (hour > 0)
    {
        oss << std::setfill('0') << std::setw(2) << hour << ':'
            << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec)
            oss << '.' << std::setw(3) << milli;
    }
    else
    {
        oss << std::setfill('0') << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec)
            oss << '.' << std::setw(3) << milli;
    }
    return oss.str();
}

static bool SequencerExpendButton(ImDrawList *draw_list, ImVec2 pos, bool expand = true)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    int delColor = IM_COL32_WHITE;
    float midy = pos.y + 16 / 2 - 0.5f;
    float midx = pos.x + 16 / 2 - 0.5f;
    draw_list->AddRect(delRect.Min, delRect.Max, delColor, 4);
    draw_list->AddLine(ImVec2(delRect.Min.x + 3, midy), ImVec2(delRect.Max.x - 4, midy), delColor, 2);
    if (expand) draw_list->AddLine(ImVec2(midx, delRect.Min.y + 3), ImVec2(midx, delRect.Max.y - 4), delColor, 2);
    return overDel;
}

static bool SequencerButton(ImDrawList *draw_list, const char * label, ImVec2 pos, ImVec2 size, ImVec4 hover_color = ImVec4(0.5f, 0.5f, 0.75f, 1.0f))
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect rect(pos, pos + size);
    bool overButton = rect.Contains(io.MousePos);
    if (overButton)
        draw_list->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(hover_color), 2);
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImGui::SetWindowFontScale(0.75);
    draw_list->AddText(pos, ImGui::GetColorU32(color), label);
    ImGui::SetWindowFontScale(1.0);
    return overButton;
}

bool Sequencer(SequencerInterface *sequencer, int64_t *currentTime, bool *expanded, int *selectedEntry, int64_t *firstTime, int64_t *lastTime, int sequenceOptions)
{

    /*************************************************************************************************************
     * [-]------------------------------------ header area ----------------------------------------------------- +
     *                    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
     * ___________________|_______________|_____________________________________________________________________ a
     s | title     [+][-] |               |          Item bar                                                    n
     l |     legend       |---------------|--------------------------------------------------------------------- v
     o |                  |               |        custom area                                                   a 
     t |                  |               |                                                                      s                                            
     s |__________________|_______________|_____________________________________________________________________ |
     * | title     [+][-] |               |          Item bar                                                    h
     * |     legend       |---------------|--------------------------------------------------------------------- e
     * |                  |               |                                                                      i
     * |                  |               |        custom area                                                   g
     * |________________________________________________________________________________________________________ h
     * | title     [+][-] |               |          Item bar  <compact view>                                    t
     * |__________________|_______________|_____________________________________________________________________ +
     *                                                                                                           +
     *                     [     <==slider==>                                                                  ] +
     ++++++++++++++++++++++++++++++++++++++++ canvas width +++++++++++++++++++++++++++++++++++++++++++++++++++++++
     *************************************************************************************************************/
    bool ret = false;
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    static float msPixelWidth = 0.1f;
    static float msPixelWidthTarget = 0.1f;
    int legendWidth = 200;
    static int movingEntry = -1;
    static int movingPos = -1;
    static int movingPart = -1;
    int delEntry = -1;
    int dupEntry = -1;
    int ItemHeight = 20;
    int HeadHeight = 20;
    int scrollBarHeight = 16;
    bool popupOpened = false;
    int itemCount = sequencer->GetItemCount();

    ImGui::BeginGroup();

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();     // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail() - ImVec2(8, 0); // Resize canvas to what's available
    int64_t firstTimeUsed = firstTime ? *firstTime : 0;
    int controlHeight = itemCount * ItemHeight;
    for (int i = 0; i < itemCount; i++)
        controlHeight += int(sequencer->GetCustomHeight(i));
    int64_t duration = ImMax(sequencer->GetEnd() - sequencer->GetStart(), (int64_t)1);
    static bool MovingScrollBar = false;
    static bool MovingCurrentTime = false;
    ImVector<SequencerCustomDraw> customDraws;
    ImVector<SequencerCustomDraw> compactCustomDraws;
    // zoom in/out
    const int64_t visibleTime = (int64_t)floorf((canvas_size.x - legendWidth) / msPixelWidth);
    const float barWidthRatio = ImMin(visibleTime / (float)duration, 1.f);
    const float barWidthInPixels = barWidthRatio * (canvas_size.x - legendWidth);
    ImRect regionRect(canvas_pos + ImVec2(0, HeadHeight), canvas_pos + canvas_size);
    static bool panningView = false;
    static ImVec2 panningViewSource;
    static int64_t panningViewTime;
    ImRect scrollBarRect;
    ImRect scrollHandleBarRect;
    if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
    {
        if (!panningView)
        {
            panningViewSource = io.MousePos;
            panningView = true;
            panningViewTime = *firstTime;
        }
        *firstTime = panningViewTime - int((io.MousePos.x - panningViewSource.x) / msPixelWidth);
        *firstTime = ImClamp(*firstTime, sequencer->GetStart(), sequencer->GetEnd() - visibleTime);
    }
    if (panningView && !io.MouseDown[2])
    {
        panningView = false;
    }

    float minPixelWidthTarget = 0.001f;
    float maxPixelWidthTarget = 20.f;
    float min_frame_duration = FLT_MAX;
    for (int i = 0; i < itemCount; i++)
    {
        float frame_duration;
        float snapshot_width;
        sequencer->Get(i, frame_duration, snapshot_width);
        if (frame_duration > 0 && snapshot_width > 0)
        {
            if (min_frame_duration > frame_duration)
            {
                min_frame_duration = frame_duration;
                maxPixelWidthTarget = floor(snapshot_width) / frame_duration;
            }
        }
    }
    msPixelWidthTarget = ImClamp(msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
    msPixelWidth = ImLerp(msPixelWidth, msPixelWidthTarget, 0.33f);
    duration = sequencer->GetEnd() - sequencer->GetStart();

    if (visibleTime >= duration && firstTime)
        *firstTime = sequencer->GetStart();
    if (lastTime)
        *lastTime = firstTimeUsed + visibleTime;

    ImGui::SetCursorScreenPos(canvas_pos + ImVec2(4, ItemHeight + 4));
    ImGui::InvisibleButton("canvas", canvas_size - ImVec2(8, ItemHeight + scrollBarHeight + 8));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
        {
            ImSequencer::SequencerItem * item = (ImSequencer::SequencerItem*)payload->Data;
            ImSequencer::MediaSequencer * seq = (ImSequencer::MediaSequencer *)sequencer;
            SequencerItem * new_item = new SequencerItem(item->mName, item->mPath, item->mStart, item->mEnd, true, item->mMediaType);
            auto length = item->mEnd - item->mStart;
            if (currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequencer->GetEnd())
                new_item->mStart = *currentTime;
            else
                new_item->mStart = *firstTime;
            new_item->mEnd = new_item->mStart + length;
            if (new_item->mEnd > sequencer->GetEnd())
                sequencer->SetEnd(new_item->mEnd + 10 * 1000);
            seq->m_Items.push_back(new_item);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorScreenPos(canvas_pos);
    if ((expanded && !*expanded) || !itemCount)
    {
        // minimum view
        ImGui::InvisibleButton("canvas_minimum", ImVec2(canvas_size.x - canvas_pos.x - 8.f, (float)ItemHeight));
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x - 8.f, canvas_pos.y + ItemHeight), COL_CANVAS_BG, 0);
        auto info_str = MillisecToString(duration, true);
        info_str += " / ";
        info_str += std::to_string(itemCount) + " entries";
        draw_list->AddText(ImVec2(canvas_pos.x + 40, canvas_pos.y + 2), IM_COL32_WHITE, info_str.c_str());
    }
    else
    {
        // normal view
        bool hasScrollBar(true);
        // test scroll area
        ImVec2 headerSize(canvas_size.x - 4.f, (float)HeadHeight);
        ImVec2 scrollBarSize(canvas_size.x, scrollBarHeight);
        ImGui::InvisibleButton("topBar", headerSize);
        draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, IM_COL32_BLACK, 0);
        if (!itemCount) 
        {
            ImGui::EndGroup();
            return false;
        }
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(canvas_size.x, canvas_size.y - 8.f - headerSize.y - (hasScrollBar ? scrollBarSize.y : 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::BeginChildFrame(889, childFrameSize, ImGuiWindowFlags_NoScrollbar); // id = 889 why?
        sequencer->focused = ImGui::IsWindowFocused();
        ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x - 8.f, float(controlHeight)));
        const ImVec2 contentMin = ImGui::GetItemRectMin();
        const ImVec2 contentMax = ImGui::GetItemRectMax();
        const ImRect contentRect(contentMin, contentMax);
        const ImRect legendRect(contentMin, ImVec2(contentMin.x + legendWidth, contentMax.y));
        const float contentHeight = contentMax.y - contentMin.y;
        
        // full canvas background
        draw_list->AddRectFilled(canvas_pos + ImVec2(4, ItemHeight + 4), canvas_pos + ImVec2(4, ItemHeight + 4) + canvas_size - ImVec2(8, ItemHeight + scrollBarHeight + 8), COL_CANVAS_BG, 0);
        // full legend background
        draw_list->AddRectFilled(legendRect.Min, legendRect.Max, COL_LEGEND_BG, 0);

        // current time top
        ImRect topRect(ImVec2(canvas_pos.x + legendWidth, canvas_pos.y), ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ItemHeight));
        if (!MovingCurrentTime && !MovingScrollBar && movingEntry == -1 && sequenceOptions & SEQUENCER_CHANGE_TIME && currentTime && *currentTime >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
        {
            MovingCurrentTime = true;
        }
        if (MovingCurrentTime)
        {
            if (duration)
            {
                *currentTime = (int)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + firstTimeUsed;
                if (*currentTime < sequencer->GetStart())
                    *currentTime = sequencer->GetStart();
                if (*currentTime >= sequencer->GetEnd())
                    *currentTime = sequencer->GetEnd();
            }
            if (!io.MouseDown[0])
                MovingCurrentTime = false;
        }

        //header
        //header time and lines
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * msPixelWidth) < 150)
        {
            modTimeCount *= 2;
            timeStep *= 2;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == sequencer->GetEnd() || i == sequencer->GetStart());
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)canvas_pos.x + int(i * msPixelWidth) + legendWidth - int(firstTimeUsed * msPixelWidth);
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : HeadHeight;
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), COL_MARK, 1);
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)HeadHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), COL_MARK, 1);
            }
            if (baseIndex && px > (canvas_pos.x + legendWidth))
            {
                auto time_str = MillisecToString(i, true);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), COL_RULE_TEXT, time_str.c_str());
            }
        };
        auto drawLineContent = [&](int64_t i, int /*regionHeight*/)
        {
            int px = (int)canvas_pos.x + int(i * msPixelWidth) + legendWidth - int(firstTimeUsed * msPixelWidth);
            int tiretStart = int(contentMin.y);
            int tiretEnd = int(contentMax.y);
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), COL_SLOT_V_LINE, 1);
            }
        };
        for (auto i = sequencer->GetStart(); i <= sequencer->GetEnd(); i += timeStep)
        {
            drawLine(i, HeadHeight);
        }
        drawLine(sequencer->GetStart(), HeadHeight);
        drawLine(sequencer->GetEnd(), HeadHeight);

        // cursor Arrow
        if (currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequencer->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - arrowWidth * 0.5f - 3;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = MillisecToString(*currentTime, true);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - str_size.x * 0.5f - 3;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }

        // clip content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);

        // slots background
        size_t customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            unsigned int col = (i & 1) ? COL_SLOT_ODD : COL_SLOT_EVEN;
            size_t localCustomHeight = sequencer->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x - 4.f, pos.y + ItemHeight - 1 + localCustomHeight);
            if (!popupOpened && cy >= pos.y && cy < pos.y + (ItemHeight + localCustomHeight) && movingEntry == -1 && cx > contentMin.x && cx < contentMin.x + canvas_size.x)
            {
                col += IM_COL32(8, 16, 32, 128);
                pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
        }
        // draw item names and control button in the legend rect on the left
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            ImVec2 button_size = ImVec2(12, 12);
            bool expanded, view, locked, muted;
            sequencer->Get(i, expanded, view, locked, muted);
            ImVec2 tpos(contentMin.x + 3, contentMin.y + i * ItemHeight + customHeight);
            draw_list->AddText(tpos, IM_COL32_WHITE, sequencer->GetItemLabel(i));
            auto itemCustomHeight = sequencer->GetCustomHeight(i);
            int button_count = 0;
            if (sequenceOptions & SEQUENCER_LOCK)
            {
                bool ret = SequencerButton(draw_list, locked ? ICON_LOCKED : ICON_UNLOCK, ImVec2(contentMin.x + button_size.x * button_count * 1.5 + 2, tpos.y + ItemHeight + itemCustomHeight - button_size.y), button_size);
                if (ret && io.MouseReleased[0])
                    locked = !locked;
                button_count ++;
            }
            if (sequenceOptions & SEQUENCER_VIEW)
            {
                bool ret = SequencerButton(draw_list, view ? ICON_VIEW : ICON_VIEW_DISABLE, ImVec2(contentMin.x + button_size.x * button_count * 1.5 + 2, tpos.y + ItemHeight + itemCustomHeight - button_size.y), button_size);
                if (ret && io.MouseReleased[0])
                    view = !view;
                button_count ++;
            }
            if (sequenceOptions & SEQUENCER_MUTE)
            {
                bool ret = SequencerButton(draw_list, muted ? ICON_SPEAKER_MUTE : ICON_SPEAKER, ImVec2(contentMin.x + button_size.x * button_count * 1.5 + 2, tpos.y + ItemHeight + itemCustomHeight - button_size.y), button_size);
                if (ret && io.MouseReleased[0])
                    muted = !muted;
                button_count ++;
            }
            if (sequenceOptions & SEQUENCER_DEL)
            {
                bool ret = SequencerButton(draw_list, ICON_TRASH, ImVec2(contentMin.x + button_size.x * button_count * 1.5 + 2, tpos.y + ItemHeight + itemCustomHeight - button_size.y), button_size);
                if (ret && io.MouseReleased[0])
                    delEntry = i;
                button_count ++;
            }
            if (sequenceOptions & SEQUENCER_ADD)
            {
                bool ret = SequencerButton(draw_list, ICON_CLONE, ImVec2(contentMin.x + button_size.x * button_count * 1.5 + 2, tpos.y + ItemHeight + itemCustomHeight - button_size.y), button_size);
                if (ret && io.MouseReleased[0])
                    dupEntry = i;
                button_count ++;
            }
            sequencer->Set(i, expanded, view, locked, muted);
            customHeight += itemCustomHeight;
        }
        // clipping rect so items bars are not visible in the legend on the left when scrolled
    
        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        // vertical time lines in content area
        for (auto i = sequencer->GetStart(); i <= sequencer->GetEnd(); i += timeStep)
        {
            drawLineContent(i, int(contentHeight));
        }
        drawLineContent(sequencer->GetStart(), int(contentHeight));
        drawLineContent(sequencer->GetEnd(), int(contentHeight));
        // selection
        bool selected = selectedEntry && (*selectedEntry >= 0);
        if (selected)
        {
            customHeight = 0;
            for (int i = 0; i < *selectedEntry; i++)
                customHeight += sequencer->GetCustomHeight(i);
            draw_list->AddRectFilled(ImVec2(contentMin.x, contentMin.y + ItemHeight * *selectedEntry + customHeight), ImVec2(contentMin.x + canvas_size.x - 8.f, contentMin.y + ItemHeight * (*selectedEntry + 1) + customHeight), COL_SLOT_SELECTED, 1.f);
        }
        
        // slots
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            int64_t start, end, length;
            int64_t start_offset, end_offset;
            std::string name;
            unsigned int color;
            std::vector<VideoSnapshotInfo> snapshots;
            sequencer->Get(i, start, end, length, start_offset, end_offset, name, color);
            size_t localCustomHeight = sequencer->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth - firstTimeUsed * msPixelWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 slotP1(pos.x + start * msPixelWidth, pos.y + 2);
            ImVec2 slotP2(pos.x + end * msPixelWidth + msPixelWidth - 4.f, pos.y + ItemHeight - 2);
            ImVec2 slotP3(pos.x + end * msPixelWidth + msPixelWidth - 4.f, pos.y + ItemHeight - 2 + localCustomHeight);
            unsigned int slotColor = color | IM_COL32_BLACK;
            unsigned int slotColorHalf = HALF_COLOR(color);
            if (slotP1.x <= (canvas_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
            {
                draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 0);
                draw_list->AddRectFilled(slotP1, slotP2, slotColor, 0);
            }
            if (ImRect(slotP1, slotP2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
            {
                sequencer->DoubleClick(i);
            }

            // Ensure grabable handles
            const float max_handle_width = slotP2.x - slotP1.x / 3.0f;
            const float min_handle_width = ImMin(10.0f, max_handle_width);
            const float handle_width = ImClamp(msPixelWidth / 2.0f, min_handle_width, max_handle_width);
            ImRect rects[3] = {ImRect(slotP1, ImVec2(slotP1.x + handle_width, slotP2.y)), ImRect(ImVec2(slotP2.x - handle_width, slotP1.y), slotP2), ImRect(slotP1, slotP2)};
            const unsigned int quadColor[] = {IM_COL32_WHITE, IM_COL32_WHITE, slotColor + (selected ? 0 : 0x202020)};
            if (movingEntry == -1 && (sequenceOptions & SEQUENCER_EDIT_STARTEND))
            {
                for (int j = 2; j >= 0; j--)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 0);
                }
                for (int j = 0; j < 3; j++)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                        continue;
                    if (ImGui::IsMouseClicked(0) && !MovingScrollBar && !MovingCurrentTime)
                    {
                        movingEntry = i;
                        movingPos = cx;
                        movingPart = j + 1;
                        sequencer->BeginEdit(movingEntry);
                        break;
                    }
                }
            }

            // calculate custom draw rect
            if (localCustomHeight > 0)
            {
                // slot normal view (custom view)
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + 1 + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - start - 0.5f) * msPixelWidth, float(ItemHeight)),
                                  rp + ImVec2(legendWidth + (end - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(localCustomHeight + ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(ItemHeight)), rp + ImVec2(canvas_size.x - 4.0f, float(localCustomHeight + ItemHeight)));
                ImRect legendRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                ImRect legendClippingRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                customDraws.push_back({i, customRect, legendRect, clippingRect, legendClippingRect});
            }
            else
            {
                // slot compact view (item bar only) 
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - sequencer->GetStart() - 0.5f) * msPixelWidth, float(0.f)),
                                  rp + ImVec2(legendWidth + (sequencer->GetEnd() - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(0.f)), rp + ImVec2(canvas_size.x, float(ItemHeight)));
                compactCustomDraws.push_back({i, customRect, ImRect(), clippingRect, ImRect()});
            }
            customHeight += localCustomHeight;
        }

        // slot moving
        if (movingEntry >= 0)
        {
            bool expanded, view, locked, muted;
            sequencer->Get(movingEntry, expanded, view, locked, muted);
            ImGui::CaptureMouseFromApp();
            int diffTime = int((cx - movingPos) / msPixelWidth);
            if (!locked && std::abs(diffTime) > 0)
            {
                int64_t start, end, length;
                int64_t start_offset, end_offset;
                std::string name;
                unsigned int color;
                float frame_duration, snapshot_width;
                sequencer->Get(movingEntry, start, end, length, start_offset, end_offset, name, color);
                sequencer->Get(movingEntry, frame_duration, snapshot_width);
                if (selectedEntry)
                    *selectedEntry = movingEntry;

                if (movingPart == 3)
                {
                    // whole slot moving
                    start += diffTime;
                    end += diffTime;
                    movingPos += int(diffTime * msPixelWidth);
                }
                else if (movingPart & 1)
                {
                    // slot left moving
                    if (start + diffTime < end - ceil(frame_duration))
                    {
                        if (start_offset + diffTime >= 0)
                        {
                            start += diffTime;
                            start_offset += diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                        else if (abs(start_offset + diffTime) <= abs(diffTime))
                        {
                            diffTime += abs(start_offset + diffTime);
                            start += diffTime;
                            start_offset += diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                    }
                    else if (end - start - ceil(frame_duration) < diffTime)
                    {
                        diffTime = end - start - ceil(frame_duration);
                        start += diffTime;
                        start_offset += diffTime;
                        movingPos += int(diffTime * msPixelWidth);
                    }
                }
                else if (movingPart & 2)
                {
                    // slot right moving
                    if (end + diffTime > start + ceil(frame_duration))
                    {
                        if (end_offset - diffTime >= 0)
                        {
                            end += diffTime;
                            end_offset -= diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                        else if (abs(end_offset - diffTime) <= abs(diffTime))
                        {
                            diffTime -= abs(end_offset - diffTime);
                            end += diffTime;
                            end_offset -= diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                    }
                    else if (end - start - ceil(frame_duration) < abs(diffTime))
                    {
                        diffTime = - (end - start - ceil(frame_duration));
                        end += diffTime;
                        end_offset -= diffTime;
                        movingPos += int(diffTime * msPixelWidth);
                    }
                }
                sequencer->Set(movingEntry, start, end, start_offset, end_offset, name, color);
            }
            if (!io.MouseDown[0])
            {
                // single select
                if (!diffTime && movingPart && selectedEntry)
                {
                    *selectedEntry = movingEntry;
                    ret = true;
                }
                movingEntry = -1;
                sequencer->EndEdit();
            }
        }

        draw_list->PopClipRect();
        draw_list->PopClipRect();
        
        // copy paste
        if (sequenceOptions & SEQUENCER_COPYPASTE)
        {
            ImRect rectCopy(ImVec2(contentMin.x + 100, canvas_pos.y + 2), ImVec2(contentMin.x + 100 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectCopy = rectCopy.Contains(io.MousePos);
            unsigned int copyColor = inRectCopy ? COL_LIGHT_BLUR : IM_COL32_BLACK;
            draw_list->AddText(rectCopy.Min, copyColor, "Copy");
            ImRect rectPaste(ImVec2(contentMin.x + 140, canvas_pos.y + 2), ImVec2(contentMin.x + 140 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectPaste = rectPaste.Contains(io.MousePos);
            unsigned int pasteColor = inRectPaste ? COL_LIGHT_BLUR : IM_COL32_BLACK;
            draw_list->AddText(rectPaste.Min, pasteColor, "Paste");
            if (inRectCopy && io.MouseReleased[0])
            {
                sequencer->Copy();
            }
            if (inRectPaste && io.MouseReleased[0])
            {
                sequencer->Paste();
            }
        }
        ImGui::EndChildFrame();
        ImGui::PopStyleColor();

        if (hasScrollBar)
        {
            auto scroll_pos = ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::SetWindowFontScale(0.55);
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 16 - 4, 0));
            if (ImGui::Button(ICON_TO_END "##slider_to_end", ImVec2(16, 16)))
            {
                *firstTime = sequencer->GetEnd() - visibleTime;
            }
            ImGui::ShowTooltipOnHover("Slider to End");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 32 - 4, 0));
            if (ImGui::Button(ICON_SLIDER_MAXIMUM "##slider_maximum", ImVec2(16, 16)))
            {
                msPixelWidthTarget = maxPixelWidthTarget;
            }
            ImGui::ShowTooltipOnHover("Maximum Slider");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 48 - 4, 0));
            if (ImGui::Button(ICON_ZOOM_IN "##slider_zoom_in", ImVec2(16, 16)))
            {
                *firstTime = int64_t(*firstTime * 0.9f);
                msPixelWidthTarget *= 1.1f;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
            ImGui::ShowTooltipOnHover("Slider Zoom In");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 64 - 4, 0));
            if (ImGui::Button(ICON_ZOOM_OUT "##slider_zoom_out", ImVec2(16, 16)))
            {
                *firstTime = int64_t(*firstTime * 1.1f);
                msPixelWidthTarget *= 0.9f;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
            ImGui::ShowTooltipOnHover("Slider Zoom Out");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 80 - 4, 0));
            if (ImGui::Button(ICON_SLIDER_MINIMUM "##slider_minimum", ImVec2(16, 16)))
            {
                msPixelWidthTarget = minPixelWidthTarget;
            }
            ImGui::ShowTooltipOnHover("Minimum Slider");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 96 - 4, 0));
            if (ImGui::Button(ICON_TO_START "##slider_to_start", ImVec2(16, 16)))
            {
                *firstTime = sequencer->GetStart();
            }
            ImGui::ShowTooltipOnHover("Slider to Start");
            ImGui::SetWindowFontScale(1.0);
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(scroll_pos);
            ImGui::InvisibleButton("scrollBar", scrollBarSize);
            ImVec2 scrollBarMin = ImGui::GetItemRectMin();
            ImVec2 scrollBarMax = ImGui::GetItemRectMax();

            // ratio = time visible in control / number to total time
            float startOffset = ((float)(firstTimeUsed - sequencer->GetStart()) / (float)duration) * (canvas_size.x - legendWidth);
            ImVec2 scrollBarA(scrollBarMin.x + legendWidth, scrollBarMin.y - 2);
            ImVec2 scrollBarB(scrollBarMin.x + canvas_size.x, scrollBarMax.y - 1);
            scrollBarRect = ImRect(scrollBarA, scrollBarB);
            bool inScrollBar = scrollBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, COL_SLIDER_BG, 8);
            ImVec2 scrollBarC(scrollBarMin.x + legendWidth + startOffset, scrollBarMin.y);
            ImVec2 scrollBarD(scrollBarMin.x + legendWidth + barWidthInPixels + startOffset, scrollBarMax.y - 2);
            scrollHandleBarRect = ImRect(scrollBarC, scrollBarD);
            bool inScrollHandle = scrollHandleBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || MovingScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
            if (MovingScrollBar)
            {
                if (!io.MouseDown[0])
                {
                    MovingScrollBar = false;
                }
                else
                {
                    float msPerPixelInBar = barWidthInPixels / (float)visibleTime;
                    *firstTime = int((io.MousePos.x - panningViewSource.x) / msPerPixelInBar) - panningViewTime;
                    *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
                }
            }
            else if (inScrollHandle && ImGui::IsMouseClicked(0) && firstTime && !MovingCurrentTime && movingEntry == -1)
            {
                MovingScrollBar = true;
                panningViewSource = io.MousePos;
                panningViewTime = -*firstTime;
            }
            else if (inScrollBar && ImGui::IsMouseReleased(0))
            {
                float msPerPixelInBar = barWidthInPixels / (float)visibleTime;
                *firstTime = int((io.MousePos.x - legendWidth - scrollHandleBarRect.GetWidth() / 2)/ msPerPixelInBar);
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
        }
    }

    if (regionRect.Contains(io.MousePos))
    {
        bool overCustomDraw = false;
        bool overScrollBar = false;
        for (auto &custom : customDraws)
        {
            if (custom.customRect.Contains(io.MousePos))
            {
                overCustomDraw = true;
            }
        }
        if (scrollBarRect.Contains(io.MousePos))
        {
            overScrollBar = true;
        }
        if (overScrollBar)
        {
            // up-down wheel over scrollbar, scale canvas view
            int64_t overCursor = *firstTime + (int64_t)(visibleTime * ((io.MousePos.x - (float)legendWidth - canvas_pos.x) / (canvas_size.x - legendWidth)));
            if (io.MouseWheel < -FLT_EPSILON && visibleTime <= sequencer->GetEnd())
            {
                *firstTime -= overCursor;
                *firstTime = int64_t(*firstTime * 1.1f);
                msPixelWidthTarget *= 0.9f;
                *firstTime += overCursor;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
            if (io.MouseWheel > FLT_EPSILON)
            {
                *firstTime -= overCursor;
                *firstTime = int64_t(*firstTime * 0.9f);
                msPixelWidthTarget *= 1.1f;
                *firstTime += overCursor;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
        }
        else
        {
            // left-right wheel over blank area, moving canvas view
            if (io.MouseWheelH < -FLT_EPSILON)
            {
                *firstTime -= visibleTime / 4;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
            if (io.MouseWheelH > FLT_EPSILON)
            {
                *firstTime += visibleTime / 4;
                *firstTime = ImClamp(*firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - visibleTime, sequencer->GetStart()));
            }
        }
    }

    for (auto &customDraw : customDraws)
        sequencer->CustomDraw(customDraw.index, draw_list, customDraw.customRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect, *firstTime, visibleTime);
    for (auto &customDraw : compactCustomDraws)
        sequencer->CustomDrawCompact(customDraw.index, draw_list, customDraw.customRect, customDraw.clippingRect, *firstTime, visibleTime);

    // cursor line
    draw_list->PushClipRect(canvas_pos + ImVec2(float(legendWidth), 0.f), canvas_pos + canvas_size);
    if (itemCount > 0 && currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequencer->GetEnd())
    {
        ImVec2 contentMin(canvas_pos.x + 4.f, canvas_pos.y + (float)HeadHeight + 8.f);
        ImVec2 contentMax(canvas_pos.x + canvas_size.x - 4.f, canvas_pos.y + (float)HeadHeight + float(controlHeight) + 8.f);
        static const float cursorWidth = 3.f;
        float cursorOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - cursorWidth * 0.5f - 2;
        draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 128), cursorWidth);
    }
    draw_list->PopClipRect();

    ImGui::EndGroup();

    if (expanded)
    {
        bool overExpanded = SequencerExpendButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded);
        if (overExpanded && io.MouseReleased[0])
            *expanded = !*expanded;
    }
    if (delEntry != -1)
    {
        sequencer->Del(delEntry);
        if (selectedEntry && (*selectedEntry == delEntry || *selectedEntry >= sequencer->GetItemCount()))
            *selectedEntry = -1;
    }
    if (dupEntry != -1)
    {
        sequencer->Duplicate(dupEntry);
    }
    return ret;
}

/***********************************************************************************************************
 * SequencerItem Struct Member Functions
 ***********************************************************************************************************/

SequencerItem::SequencerItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type)
{
    mName = name;
    mPath = path;
    mStart = start;
    mEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mMedia = CreateMediaSnapshot();
    mColor = COL_SLOT_DEFAULT;
    mTotalFrame = 0;
    if (!path.empty() && mMedia)
    {
        mMedia->Open(path);
    }
    if (mMedia && mMedia->IsOpened())
    {
        double window_size = 1.0f;
        mLength = mEnd = mMedia->GetVidoeDuration();
        mMedia->SetCacheFactor(2.0);
        mMedia->SetSnapshotResizeFactor(0.25, 0.25);
        mMedia->ConfigSnapWindow(window_size, 10);
    }
}

SequencerItem::~SequencerItem()
{
    ReleaseMediaSnapshot(&mMedia);
    mMedia = nullptr;
    if (mMediaThumbnail)
    {
        ImGui::ImDestroyTexture(mMediaThumbnail); 
        mMediaThumbnail = nullptr;
    }
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) ImGui::ImDestroyTexture(snap.texture); 
    }
}

void SequencerItem::SequencerItemUpdateThumbnail()
{
    if (mMediaThumbnail)
        return;
    if (mMedia && mMedia->IsOpened())
    {
        auto pos = (float)mMedia->GetVidoeMinPos()/1000.f;
        std::vector<ImGui::ImMat> snapshots;
        if (mMedia->GetSnapshots(snapshots, pos))
        {
            auto snap = snapshots[snapshots.size() / 2];
            if (!snap.empty())
            {
                if (snap.device == ImDataDevice::IM_DD_CPU)
                {
                    ImGui::ImGenerateOrUpdateTexture(mMediaThumbnail, snap.w, snap.h, snap.c, (const unsigned char *)snap.data);
                }
#if IMGUI_VULKAN_SHADER
                if (snap.device == ImDataDevice::IM_DD_VULKAN)
                {
                    ImGui::VkMat vkmat = snap;
                    ImGui::ImGenerateOrUpdateTexture(mMediaThumbnail, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                }
#endif
            }
        }
    }
}

void SequencerItem::SequencerItemUpdateSnapshots()
{
    if (mMedia && mMedia->IsOpened())
    {
        std::vector<ImGui::ImMat> snapshots;
        double pos = (double)(mSnapshotPos + mStartOffset) / 1000.f;
        if (mMedia->GetSnapshots(snapshots, pos))
        {
            for (int i = 0; i < snapshots.size(); i++)
            {
                if (!snapshots[i].empty())
                {
                    // if i <= mVideoSnapshots.size() then update text else create a new text and push back into mVideoSnapshots
                    if (i < mVideoSnapshots.size())
                    {
                        if (mVideoSnapshots[i].time_stamp != (int64_t)(snapshots[i].time_stamp * 1000) || !mVideoSnapshots[i].available)
                        {
                            if (snapshots[i].device == ImDataDevice::IM_DD_CPU)
                            {
                                ImGui::ImGenerateOrUpdateTexture(mVideoSnapshots[i].texture, snapshots[i].w, snapshots[i].h, snapshots[i].c, (const unsigned char *)snapshots[i].data);
                            }
#if IMGUI_VULKAN_SHADER
                            if (snapshots[i].device == ImDataDevice::IM_DD_VULKAN)
                            {
                                ImGui::VkMat vkmat = snapshots[i];
                                ImGui::ImGenerateOrUpdateTexture(mVideoSnapshots[i].texture, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                            }
#endif
                            mVideoSnapshots[i].time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                            mVideoSnapshots[i].available = true;
                        }
                    }
                    else
                    {
                        Snapshot snap;
                        if (snapshots[i].device == ImDataDevice::IM_DD_CPU)
                        {
                            ImGui::ImGenerateOrUpdateTexture(snap.texture, snapshots[i].w, snapshots[i].h, snapshots[i].c, (const unsigned char *)snapshots[i].data);
                        }
#if IMGUI_VULKAN_SHADER
                        if (snapshots[i].device == ImDataDevice::IM_DD_VULKAN)
                        {
                            ImGui::VkMat vkmat = snapshots[i];
                            ImGui::ImGenerateOrUpdateTexture(snap.texture, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                        }
#endif
                        snap.time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                        snap.available = true;
                        mVideoSnapshots.push_back(snap);
                    }
                }
                else
                {
                    // do we need clean texture cache?
                    if (i < mVideoSnapshots.size())
                    {
                        auto snap = mVideoSnapshots.begin() + i;
                        //if (snap->texture) { ImGui::ImDestroyTexture(snap->texture); snap->texture = nullptr; }
                        snap->available = false;
                        snap->time_stamp = 0;
                        snap->estimate_time = 0;
                    }
                }
            }
        }
        else
        {
            for (auto& snap : mVideoSnapshots)
            {
                snap.available = false;
            }
        }
    }
}

void SequencerItem::CalculateVideoSnapshotInfo(const ImRect &customRect, int64_t viewStartTime, int64_t visibleTime)
{
    if (mMedia && mMedia->IsOpened() && mMedia->HasVideo())
    {
        auto width = mMedia->GetVideoWidth();
        auto height = mMedia->GetVideoHeight();
        auto duration = mMedia->GetVidoeDuration();
        auto total_frames = mMedia->GetVidoeFrameCount();
        auto clip_duration = mEnd - mStart;
        if (!width || !height || !duration || !total_frames)
            return;
        if (customRect.GetHeight() <= 0 || customRect.GetWidth() <= 0)
            return;
        float aspio = (float)width / (float)height;
        float snapshot_height = customRect.GetHeight();
        float snapshot_width = snapshot_height * aspio;
        mSnapshotWidth = snapshot_width;
        mFrameDuration = (float)duration / (float)total_frames;
        float frame_count = (customRect.GetWidth() + snapshot_width) / snapshot_width;
        float snapshot_duration = (float)clip_duration / (float)(frame_count - 1);
        mMaxViewSnapshot = (int)((visibleTime + snapshot_duration) / snapshot_duration);
        if (frame_count != mTotalFrame || (int)frame_count != mVideoSnapshotInfos.size())
        {
            mVideoSnapshotInfos.clear();
            for (auto& snap : mVideoSnapshots)
            {
                if (snap.texture) ImGui::ImDestroyTexture(snap.texture);
            }
            mVideoSnapshots.clear();
            mSnapshotPos = -1;
            double window_size = visibleTime / 1000.0;
            mMedia->ConfigSnapWindow(window_size, mMaxViewSnapshot);
            mTotalFrame = frame_count;
            for (int i = 0; i < (int)frame_count; i++)
            {
                VideoSnapshotInfo snapshot;
                snapshot.rc.Min = ImVec2(i * snapshot_width, 0);
                snapshot.rc.Max = ImVec2((i + 1) * snapshot_width, snapshot_height);
                if (snapshot.rc.Max.x > customRect.GetWidth() + 2)
                    snapshot.rc.Max.x = customRect.GetWidth() + 2;
                snapshot.time_stamp = i * snapshot_duration + mStartOffset;
                snapshot.duration = snapshot_duration;
                snapshot.frame_width = snapshot.rc.Max.x - snapshot.rc.Min.x;
                mVideoSnapshotInfos.push_back(snapshot);
            }
        }
    }
}

/***********************************************************************************************************
 * MediaSequencer Struct Member Functions
 ***********************************************************************************************************/

MediaSequencer::~MediaSequencer()
{
    for (auto item : m_Items)
    {
        delete item;
    }
}

void MediaSequencer::Get(int index, int64_t& start, int64_t& end, int64_t& length, int64_t& start_offset, int64_t& end_offset, std::string& name, unsigned int& color)
{
    SequencerItem *item = m_Items[index];
    color = item->mColor;
    start = item->mStart;
    end = item->mEnd;
    length = item->mLength;
    start_offset = item->mStartOffset;
    end_offset = item->mEndOffset;
    name = item->mName;
}

void MediaSequencer::Get(int index, float& frame_duration, float& snapshot_width)
{
    SequencerItem *item = m_Items[index];
    frame_duration = item->mFrameDuration;
    snapshot_width = item->mSnapshotWidth;
}

void MediaSequencer::Get(int index, bool& expanded, bool& view, bool& locked, bool& muted)
{
    SequencerItem *item = m_Items[index];
    expanded = item->mExpanded;
    view = item->mView;
    locked = item->mLocked;
    muted = item->mMuted;
}

void MediaSequencer::Set(int index, int64_t start, int64_t end, int64_t start_offset, int64_t end_offset, std::string name, unsigned int color)
{
    SequencerItem *item = m_Items[index];
    item->mColor = color;
    item->mStart = start;
    item->mEnd = end;
    item->mName = name;
    item->mStartOffset = start_offset;
    item->mEndOffset = end_offset;
}

void MediaSequencer::Set(int index, bool expanded, bool view, bool locked, bool muted)
{
    SequencerItem *item = m_Items[index];
    item->mExpanded = expanded;
    item->mView = view;
    item->mLocked = locked;
    item->mMuted = muted;
}

void MediaSequencer::Add(std::string& name)
{ 
    /*m_Items.push_back(SequencerItem{name, "", 0, 10, false});*/ 
}
    
void MediaSequencer::Del(int index)
{
    auto item = m_Items.erase(m_Items.begin() + index);
    delete *item;
}
    
void MediaSequencer::Duplicate(int index)
{
    /*m_Items.push_back(m_Items[index]);*/
}

void MediaSequencer::CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime)
{
    // rc: full item length rect
    // clippingRect: current view window area
    // legendRect: legend area
    // legendClippingRect: legend area

    SequencerItem *item = m_Items[index];
    item->CalculateVideoSnapshotInfo(rc, viewStartTime, visibleTime);
    if (item->mVideoSnapshotInfos.size()  == 0) return;
    float frame_width = item->mVideoSnapshotInfos[0].frame_width;
    int64_t lendth = item->mEnd - item->mStart;
    int64_t startTime = viewStartTime - item->mStart;
    if (startTime < 0) startTime = 0;
    if (startTime > lendth) startTime = lendth;
    int total_snapshot = item->mVideoSnapshotInfos.size();
    int snapshot_index = floor((float)startTime / (float)lendth * (float)total_snapshot);
    
    int max_snapshot = (clippingRect.GetWidth() + frame_width / 2) / frame_width + 1; 
    int snapshot_count = (snapshot_index + max_snapshot < total_snapshot) ? max_snapshot : total_snapshot - snapshot_index;
    if (item->mSnapshotPos != startTime)
    {
        item->mSnapshotPos = startTime;
        item->SequencerItemUpdateSnapshots();
    }
    else
        item->SequencerItemUpdateSnapshots();

    // draw snapshot
    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    for (int i = 0; i < snapshot_count; i++)
    {
        if (i + snapshot_index > total_snapshot - 1) break;
        int64_t time_stamp = item->mVideoSnapshotInfos[snapshot_index + i].time_stamp;
        ImRect frame_rc = item->mVideoSnapshotInfos[snapshot_index + i].rc;
        ImVec2 pos = frame_rc.Min + rc.Min;
        ImVec2 size = frame_rc.Max - frame_rc.Min;
        if (i < item->mVideoSnapshots.size() && item->mVideoSnapshots[i].texture && item->mVideoSnapshots[i].available)
        {
            item->mVideoSnapshots[i].estimate_time = time_stamp;
            // already got snapshot
            ImGui::SetCursorScreenPos(pos);
            if (i + snapshot_index == total_snapshot - 1)
            {
                // last frame of media, we need clip frame
                float width_clip = size.x / frame_width;
                ImGui::Image(item->mVideoSnapshots[i].texture, ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
            }
            else if (pos.x + size.x < clippingRect.Max.x)
                ImGui::Image(item->mVideoSnapshots[i].texture, size);
            else if (pos.x < clippingRect.Max.x)
            {
                // last frame of view range, we need clip frame
                float width_clip = (clippingRect.Max.x - pos.x) / size.x;
                ImGui::Image(item->mVideoSnapshots[i].texture, ImVec2(clippingRect.Max.x - pos.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
            }
            time_stamp = item->mVideoSnapshots[i].time_stamp;
        }
        else
        {
            // not got snapshot, we show circle indicalor
            //draw_list->AddRect(pos, pos + size, IM_COL32_BLACK);
            draw_list->AddRectFilled(pos, pos + size, IM_COL32_BLACK);
            auto center_pos = pos + size / 2;
            ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
            ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
            ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
            ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
        }
        
        auto time_string = MillisecToString(time_stamp, true);
        ImGui::SetWindowFontScale(0.7);
        ImVec2 str_size = ImGui::CalcTextSize(time_string.c_str(), nullptr, true);
        if (str_size.x <= size.x)
            draw_list->AddText(frame_rc.Min + rc.Min + ImVec2(2, 48), IM_COL32_WHITE, time_string.c_str());
        ImGui::SetWindowFontScale(1.0);
    }
    
    // for Debug: print some info here 
    //draw_list->AddText(clippingRect.Min + ImVec2(2, 8), IM_COL32_WHITE, std::to_string(item->mStartOffset).c_str());
    //draw_list->AddText(clippingRect.Min + ImVec2(2, 24), IM_COL32_WHITE, std::to_string(item->mEndOffset).c_str());
    draw_list->PopClipRect();

    // draw legend
    draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
    //draw_list->AddRectFilled(legendClippingRect.Min, legendClippingRect.Max, IM_COL32(255,0, 0, 128), 0);
    // draw media control
    draw_list->PopClipRect();
    //ImGui::SetCursorScreenPos(rc.Min);
}

void MediaSequencer::CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect, int64_t viewStartTime, int64_t visibleTime)
{
    // rc: full item length rect
    // clippingRect: current view window area

    //draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    //draw_list->PopClipRect();
    //draw_list->AddRectFilled(clippingRect.Min, clippingRect.Max, IM_COL32(255,0, 0, 128), 0);
}

void MediaSequencer::GetVideoSnapshotInfo(int index, std::vector<VideoSnapshotInfo>& snapshots)
{
    SequencerItem *item = m_Items[index];
    snapshots = item->mVideoSnapshotInfos;
}

} // namespace ImSequencer
