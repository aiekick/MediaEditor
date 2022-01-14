#include <application.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <imgui_knob.h>
#include <ImGuiFileDialog.h>
#include <ImGuiTabWindow.h>
#include "ImSequencer.h"
#include "FFUtils.h"
#include "Logger.h"
#include <sstream>

using namespace ImSequencer;

static std::string bookmark_path = "bookmark.ini";
static std::string ini_file = "Media_Editor.ini";

static const char* ControlPanelTabNames[] = {
    ICON_MEDIA_BANK,
    ICON_MEDIA_TRANS,
    ICON_MEDIA_FILTERS,
    ICON_MEDIA_OUTPUT
};

static const char* ControlPanelTabTooltips[] = 
{
    "Meida Bank",
    "Transition Bank",
    "Filters Bank",
    "Meida Output"
};

static const char* MainWindowTabNames[] = {
    ICON_MEDIA_PREVIEW,
    ICON_MEDIA_VIDEO,
    ICON_TRANS,
    ICON_MUSIC,
    ICON_MEDIA_DIAGNOSIS,
    ICON_BRAIN
};

static const char* MainWindowTabTooltips[] = 
{
    "Meida Preview",
    "Video Editor",
    "Video Fusion",
    "Audio Editor",
    "Meida Analyse",
    "Meida AI"
};


static const char* VideoEditorTabNames[] = {
    ICON_BLUE_PRINT,
    ICON_PALETTE,
    ICON_CROP,
    ICON_ROTATE
};

static const char* VideoEditorTabTooltips[] = {
    "Video Filter",
    "Video Color",
    "Video Crop",
    "Video Rotate"
};


static MediaSequencer * sequencer = nullptr;
static std::vector<MediaItem *> media_items;
static ImGui::TabLabelStyle * tab_style = &ImGui::TabLabelStyle::Get();

static inline std::string GetVideoIcon(int width, int height)
{
    if (width == 320 && height == 240) return "QVGA";
    else if (width == 176 && height == 144) return "QCIF";
    else if (width == 352 && height == 288) return "CIF";
    else if ((width == 720 && height == 576) || (width == 704 && height == 576)) return "D1";
    else if (width == 640 && height == 480) return "VGA";
    else if (width == 1280 && height == 720) return ICON_1K;
    else if (height >= 1080 && height <= 1088) return ICON_2K;
    else if (height == 1836) return ICON_3K;
    else if (height == 2160) return ICON_4K_PLUS;
    else if (height == 2700) return ICON_5K;
    else if (height == 3240) return ICON_6K;
    else if (height == 3780) return ICON_7K;
    else if (height == 4320) return ICON_8K;
    else if (height == 4860) return ICON_9K;
    else if (height == 5400) return ICON_10K;
    else 
    {
        if (height > 720  && height < 1080) return ICON_1K_PLUS;
        if (height > 1088  && height < 1836) return ICON_2K_PLUS;
        if (height > 1836  && height < 2160) return ICON_3K_PLUS;
        if (height > 2160  && height < 2700) return ICON_4K_PLUS;
        if (height > 2700  && height < 3240) return ICON_5K_PLUS;
        if (height > 3240  && height < 3780) return ICON_6K_PLUS;
        if (height > 3780  && height < 4320) return ICON_7K_PLUS;
        if (height > 4320  && height < 4860) return ICON_8K_PLUS;
        if (height > 4860  && height < 5400) return ICON_9K_PLUS;
    }
    return ICON_MEDIA_VIDEO;
}

static inline std::string GetAudioChannelName(int channels)
{
    if (channels < 2) return "Mono";
    else if (channels == 2) return "Stereo";
    else if (channels == 6) return "Surround 5.1";
    else if (channels == 8) return "Surround 7.1";
    else if (channels == 10) return "Surround 9.1";
    else if (channels == 13) return "Surround 12.1";
    else return "Channels " + std::to_string(channels);
}

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 1.0, 0.01);
}

void ShowVideoWindow(ImTextureID texture, ImVec2& pos, ImVec2& size)
{
    if (texture)
    {
        bool bViewisLandscape = size.x >= size.y ? true : false;
        float aspectRatio = (float)ImGui::ImGetTextureWidth(texture) / (float)ImGui::ImGetTextureHeight(texture);
        bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? size.y : size.x;
        float adj_h = bNeedChangeScreenInfo ? size.x : size.y;
        float adj_x = adj_h * aspectRatio;
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        float offset_x = pos.x + (size.x - adj_x) / 2.0;
        float offset_y = pos.y + (size.y - adj_y) / 2.0;
        ImGui::GetWindowDrawList()->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
    }
}

static void ShowVideoWindow(ImTextureID texture, ImVec2& pos, ImVec2& size, float& offset_x, float& offset_y, float& tf_x, float& tf_y)
{
    if (texture)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long)texture)).c_str(), size);
        bool bViewisLandscape = size.x >= size.y ? true : false;
        float aspectRatio = (float)ImGui::ImGetTextureWidth(texture) / (float)ImGui::ImGetTextureHeight(texture);
        bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? size.y : size.x;
        float adj_h = bNeedChangeScreenInfo ? size.x : size.y;
        float adj_x = adj_h * aspectRatio;
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        tf_x = (size.x - adj_x) / 2.0;
        tf_y = (size.y - adj_y) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        ImGui::GetWindowDrawList()->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
    }
}

/****************************************************************************************
 * 
 * Media Bank window
 *
 ***************************************************************************************/
static void ShowMediaBankWindow(ImDrawList *draw_list, float media_icon_size)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DARK_ONE);
    ImGui::SetWindowFontScale(2.5);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2, 0.2, 0.2, 0.7));
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    ImGui::TextUnformatted("Meida");
    ImGui::TextUnformatted("Bank");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
    ImGui::SetCursorScreenPos(window_pos);
    // Show Media Icons
    float x_offset = window_pos.x;
    for (auto item = media_items.begin(); item != media_items.end();)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        (*item)->UpdateThumbnail();
        ImGui::Dummy(ImVec2(0, 24));
        if (x_offset > 0)
        {
            ImGui::Dummy(ImVec2(x_offset, 0)); ImGui::SameLine();
        }

        auto icon_pos = ImGui::GetCursorScreenPos();
        ImVec2 icon_size = ImVec2(media_icon_size, media_icon_size);
        ImTextureID texture = nullptr;
        // Draw Shadow for Icon
        draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(32, 32, 32, 255));
        draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(48, 48, 72, 255));
        draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255));
        ImGui::InvisibleButton((*item)->mPath.c_str(), icon_size);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("Media_drag_drop", *item, sizeof(MediaItem));
            ImGui::TextUnformatted((*item)->mName.c_str());
            if (!(*item)->mMediaThumbnail.empty() && (*item)->mMediaThumbnail[0])
            {
                auto tex_w = ImGui::ImGetTextureWidth((*item)->mMediaThumbnail[0]);
                auto tex_h = ImGui::ImGetTextureHeight((*item)->mMediaThumbnail[0]);
                float aspectRatio = (float)tex_w / (float)tex_h;
                ImGui::Image((*item)->mMediaThumbnail[0], ImVec2(icon_size.x, icon_size.y / aspectRatio));
            }
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered())
        {
            float pos_x = io.MousePos.x - icon_pos.x;
            float percent = pos_x / icon_size.x;
            ImClamp(percent, 0.0f, 1.0f);
            int texture_index = (*item)->mMediaThumbnail.size() * percent;
            if (!(*item)->mMediaThumbnail.empty())
            {
                texture = (*item)->mMediaThumbnail[texture_index];
            }
        }
        else if (!(*item)->mMediaThumbnail.empty())
        {
            if ((*item)->mMediaThumbnail.size() > 1)
                texture = (*item)->mMediaThumbnail[1];
            else
                texture = (*item)->mMediaThumbnail[0];
        }
        
        ImGui::SetCursorScreenPos(icon_pos);
        if (texture)
        {
            auto tex_w = ImGui::ImGetTextureWidth(texture);
            auto tex_h = ImGui::ImGetTextureHeight(texture);
            float aspectRatio = (float)tex_w / (float)tex_h;
            bool bViewisLandscape = icon_size.x >= icon_size.y ? true : false;
            bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
            bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
            float adj_w = bNeedChangeScreenInfo ? icon_size.y : icon_size.x;
            float adj_h = bNeedChangeScreenInfo ? icon_size.x : icon_size.y;
            float adj_x = adj_h * aspectRatio;
            float adj_y = adj_h;
            if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
            float offset_x = (icon_size.x - adj_x) / 2.0;
            float offset_y = (icon_size.y - adj_y) / 2.0;
            ImGui::PushID((void*)(intptr_t)texture);
            const ImGuiID id = ImGui::GetCurrentWindow()->GetID("#image");
            ImGui::PopID();
            ImGui::ImageButtonEx(id, texture, ImVec2(adj_w - offset_x * 2, adj_h - offset_y * 2), 
                                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec2(offset_x, offset_y),
                                ImVec4(0.0f, 0.0f, 0.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            ImGui::Button((*item)->mName.c_str(), ImVec2(media_icon_size, media_icon_size));
        }

        if ((*item)->mMediaOverview && (*item)->mMediaOverview->IsOpened())
        {
            auto has_video = (*item)->mMediaOverview->HasVideo();
            auto has_audio = (*item)->mMediaOverview->HasAudio();
            auto media_length = (*item)->mMediaOverview->GetMediaParser()->GetMediaInfo()->duration;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(4, 4));
            std::string type_string = "? ";
            switch ((*item)->mMediaType)
            {
                case SEQUENCER_ITEM_UNKNOWN: break;
                case SEQUENCER_ITEM_VIDEO: type_string = std::string(ICON_FA5_FILE_VIDEO) + " "; break;
                case SEQUENCER_ITEM_AUDIO: type_string = std::string(ICON_FA5_FILE_AUDIO) + " "; break;
                case SEQUENCER_ITEM_PICTURE: type_string = std::string(ICON_FA5_FILE_IMAGE) + " "; break;
                case SEQUENCER_ITEM_TEXT: type_string = std::string(ICON_FA5_FILE_CODE) + " "; break;
                default: break;
            }
            type_string += TimestampToString(media_length);
            ImGui::TextUnformatted(type_string.c_str());
            ImGui::ShowTooltipOnHover("%s", (*item)->mPath.c_str());
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size - 20));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            if (has_video)
            {
                auto stream = (*item)->mMediaOverview->GetVideoStream();
                if (stream)
                {
                    auto video_icon = GetVideoIcon(stream->width, stream->height);
                    ImGui::SetWindowFontScale(1.2);
                    ImGui::Button(video_icon.c_str(), ImVec2(24, 24));
                    if (ImGui::IsItemHovered())
                    {
                        std::string bitrate_str = stream->bitRate >= 1000000 ? std::to_string((float)stream->bitRate / 1000000) + "Mbps" :
                                                stream->bitRate >= 1000 ? std::to_string((float)stream->bitRate / 1000) + "Kbps" :
                                                std::to_string(stream->bitRate) + "bps";
                        ImGui::BeginTooltip();
                        ImGui::Text("S: %d x %d", stream->width, stream->height);
                        ImGui::Text("B: %s", bitrate_str.c_str());
                        ImGui::Text("F: %.3ffps", stream->avgFrameRate.den > 0 ? stream->avgFrameRate.num / stream->avgFrameRate.den : 0.0);
                        ImGui::EndTooltip();
                    }
                    ImGui::SameLine(0, 0);
                    if (stream->isHdr)
                    {
                        ImGui::Button(ICON_HDR, ImVec2(24, 24));
                        ImGui::SameLine(0, 0);
                    }
                    ImGui::SetWindowFontScale(0.6);
                    ImGui::Button((std::to_string(stream->bitDepth) + "bit").c_str(), ImVec2(24, 24));
                    ImGui::SetWindowFontScale(1.0);
                    ImGui::SameLine(0, 0);
                }
            }
            if (has_audio)
            {
                auto stream = (*item)->mMediaOverview->GetAudioStream();
                if (stream)
                {
                    auto audio_channels = stream->channels;
                    auto audio_sample_rate = stream->sampleRate;
                    std::string audio_icon = audio_channels >= 2 ? ICON_STEREO : ICON_MONO;
                    ImGui::Button(audio_icon.c_str(), ImVec2(24, 24));
                    ImGui::ShowTooltipOnHover("%d %s", audio_sample_rate, GetAudioChannelName(audio_channels).c_str());
                    ImGui::SameLine(0 ,0);
                }
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SetCursorScreenPos(icon_pos + ImVec2(media_icon_size - 24, 0));
        ImGui::Button((std::string(ICON_TRASH "##delete_media") + (*item)->mPath).c_str(), ImVec2(24, 24));
        ImRect button_rect(icon_pos + ImVec2(media_icon_size - 24, 0), icon_pos + ImVec2(media_icon_size - 24, 0) + ImVec2(24, 24));
        bool overButton = button_rect.Contains(io.MousePos);
        if (overButton && io.MouseClicked[0])
        {
            // TODO::Dicky need delete it from sequencer item list ?
            MediaItem * it = *item;
            delete it;
            item = media_items.erase(item);
        }
        else
            item++;
        ImGui::ShowTooltipOnHover("Delete Media");
        ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size));
        ImGui::PopStyleColor();
    }
}

/****************************************************************************************
 * 
 * Transition Bank window
 *
 ***************************************************************************************/
static void ShowTransitionBankWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Transition Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

/****************************************************************************************
 * 
 * Transition Bank window
 *
 ***************************************************************************************/
static void ShowFilterBankWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Filter Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

/****************************************************************************************
 * 
 * Media Output window
 *
 ***************************************************************************************/
static void ShowMediaOutputWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Output");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

/****************************************************************************************
 * 
 * Media Preview window
 *
 ***************************************************************************************/
static void ShowMediaPreviewWindow(ImDrawList *draw_list)
{
    // preview control pannel
    ImVec2 PanelBarPos;
    ImVec2 PanelBarSize;
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    PanelBarPos = window_pos + window_size - ImVec2(window_size.x, 48);
    PanelBarSize = ImVec2(window_size.x, 48);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);
    
    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + 8;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32 - 8 - 32 - 8 - 32, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##preview_tostart", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->ToStart();
    }
    ImGui::ShowTooltipOnHover("To Start");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32 - 8 - 32, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##preview_step_backward", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->Step(false);
    }
    ImGui::ShowTooltipOnHover("Step Prev");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32, PanelButtonY));
    if (ImGui::Button(ICON_FAST_BACKWARD "##preview_reverse", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->Play(true, false);
    }
    ImGui::ShowTooltipOnHover("Reverse");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##preview_stop", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->Play(false, true);
    }
    ImGui::ShowTooltipOnHover("Stop");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8, PanelButtonY));
    if (ImGui::Button(ICON_FAST_FORWARD "##preview_play", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->Play(true, true);
    }
    ImGui::ShowTooltipOnHover("Play");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 32 + 8, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##preview_step_forward", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->Step(true);
    }
    ImGui::ShowTooltipOnHover("Step Next");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 32 + 8 + 32 + 8, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##preview_toend", ImVec2(32, 32)))
    {
        if (sequencer)
            sequencer->ToEnd();
    }
    ImGui::ShowTooltipOnHover("To End");

    bool loop = sequencer ? sequencer->bLoop : false;
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 32 + 8 + 32 + 8 + 32 + 8, PanelButtonY));
    if (ImGui::Button(loop ? ICON_LOOP : ICON_LOOP_ONE "##preview_loop", ImVec2(32, 32)))
    {
        if (sequencer)
        {
            loop = !loop;
            sequencer->Loop(loop);
        }
    }
    ImGui::ShowTooltipOnHover("Loop");
    ImGui::PopStyleColor(3);

    // Time stamp on left of control panel
    auto PanelRightX = PanelBarPos.x + window_size.x - 150;
    auto PanelRightY = PanelBarPos.y + 8;
    auto time_str = MillisecToString(sequencer->currentTime);
    ImGui::SetWindowFontScale(1.5);
    draw_list->AddText(ImVec2(PanelRightX, PanelRightY), sequencer->bPlay ? COL_MARK : COL_MARK_HALF, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // audio meters
    ImVec2 AudioMeterPos;
    ImVec2 AudioMeterSize;
    AudioMeterPos = window_pos + ImVec2(window_size.x - 64, 16);
    AudioMeterSize = ImVec2(32, window_size.y - 48 - 16 - 8);
    ImVec2 AudioUVLeftPos = AudioMeterPos + ImVec2(36, 0);
    ImVec2 AudioUVLeftSize = ImVec2(12, AudioMeterSize.y);
    ImVec2 AudioUVRightPos = AudioMeterPos + ImVec2(36 + 16, 0);
    ImVec2 AudioUVRightSize = AudioUVLeftSize;

    draw_list->AddRectFilled(AudioMeterPos - ImVec2(0, 16), AudioMeterPos + ImVec2(64, AudioMeterSize.y + 8), COL_DARK_TWO);

    for (int i = 0; i <= 96; i+= 5)
    {
        float mark_step = AudioMeterSize.y / 96.0f;
        ImVec2 MarkPos = AudioMeterPos + ImVec2(0, i * mark_step);
        if (i % 10 == 0)
        {
            std::string mark_str = i == 0 ? "  0" : "-" + std::to_string(i);
            draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
            ImGui::SetWindowFontScale(0.75);
            draw_list->AddText(MarkPos + ImVec2(0, 2), COL_MARK_HALF, mark_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }
        else
        {
            draw_list->AddLine(MarkPos + ImVec2(25, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
        }
    }

    static int left_stack = 0;
    static int left_count = 0;
    static int right_stack = 0;
    static int right_count = 0;
    int l_level = sequencer->GetAudioLevel(0);
    int r_level = sequencer->GetAudioLevel(1);
    ImGui::SetCursorScreenPos(AudioUVLeftPos);
    ImGui::UvMeter("##luv", AudioUVLeftSize, &l_level, 0, 96, AudioUVLeftSize.y / 4, &left_stack, &left_count);
    ImGui::SetCursorScreenPos(AudioUVRightPos);
    ImGui::UvMeter("##ruv", AudioUVRightSize, &r_level, 0, 96, AudioUVRightSize.y / 4, &right_stack, &right_count);

    // video texture area
    ImVec2 PreviewPos;
    ImVec2 PreviewSize;
    PreviewPos = window_pos + ImVec2(8, 8);
    PreviewSize = window_size - ImVec2(16 + 64, 16 + 48);
    auto frame = sequencer->GetPreviewFrame();
    if (!frame.empty())
    {
        ImGui::ImMatToTexture(frame, sequencer->mMainPreviewTexture);
    }
    ShowVideoWindow(sequencer->mMainPreviewTexture, PreviewPos, PreviewSize);
    
    ImGui::SetWindowFontScale(2);
    ImGui::SetCursorScreenPos(window_pos + ImVec2(40, 30));
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.1f);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(2, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 0.2));
    ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0.0, 0.0, 0.0, 0.8));
    ImGui::TextUnformatted("Preview");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::SetWindowFontScale(1.0);
}

/****************************************************************************************
 * 
 * Video Editor windows
 *
 ***************************************************************************************/
static ClipInfo * find_clip_with_id(int64_t id)
{
    if (!sequencer) return nullptr;
    for (auto item : sequencer->m_Items)
    {
        for (auto clip : item->mClips)
        {
            if (clip->mID == id)
            {
                return clip;
            }
        }
    }
    return nullptr;
}

static void ShowVideoBluePrintWindow(ImDrawList *draw_list, ClipInfo * clip)
{
    if (sequencer && sequencer->video_filter_bp)
    {
        sequencer->video_filter_bp->Frame(true, true, clip != nullptr);
    }
}

static void ShowVideoColorWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Video Color");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowVideoCropWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Video Crop");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowVideoRotateWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Video Rotate");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowVideoEditorWindow(ImDrawList *draw_list)
{
    static int VideoEditorWindowIndex = 0;
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float clip_timeline_height = 100;
    float editor_main_height = window_size.y - clip_timeline_height - 4;
    int selected_item = -1;
    float labelWidth = ImGui::CalcVerticalTabLabelsWidth() + 4;
    float video_view_width = window_size.x / 3;
    float video_editor_width = window_size.x - video_view_width - labelWidth;
    static int64_t last_clip = -1; 
    ClipInfo * selected_clip = nullptr;
    if (sequencer)
    {
        sequencer->Play(false, true);
        sequencer->mSequencerLock.lock();
        selected_item = sequencer->selectedEntry;
        sequencer->mSequencerLock.unlock();
        if (selected_item != -1 && selected_item < sequencer->m_Items.size())
        {
            SequencerItem * item = sequencer->m_Items[selected_item];
            for (auto clip : item->mClips)
            {
                if (clip->mSelected)
                {
                    selected_clip = clip;
                    break;
                }
            }
            if (!selected_clip)
                selected_clip = item->mClips[0];
        }
        if (selected_clip && last_clip != -1 && last_clip != selected_clip->mID)
        {
            // first find last select clip
            auto clip = find_clip_with_id(last_clip);
            if (clip && sequencer->video_filter_bp && sequencer->video_filter_bp->m_Document)
            {
                // save current BP document to last clip
                clip->mFilterBP = sequencer->video_filter_bp->m_Document->Serialize();
                sequencer->video_filter_bp->File_New(selected_clip->mFilterBP, ImVec2(video_editor_width, editor_main_height));
            }
        }
        else if (selected_clip && sequencer->video_filter_bp && last_clip == -1)
        {
            sequencer->video_filter_bp->File_New(selected_clip->mFilterBP, ImVec2(video_editor_width, editor_main_height));
        }
        if (selected_clip)
        {
            last_clip = selected_clip->mID;
        }
    }
    if (ImGui::BeginChild("##video_editor_main", ImVec2(window_size.x, editor_main_height), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 clip_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 clip_window_size = ImGui::GetWindowSize();
        static const int numTabs = sizeof(VideoEditorTabNames)/sizeof(VideoEditorTabNames[0]);
        ImGui::TabLabelsVertical(false, numTabs, VideoEditorTabNames, VideoEditorWindowIndex, VideoEditorTabTooltips, nullptr, nullptr, false, false, nullptr, nullptr);
        ImGui::SetCursorScreenPos(clip_window_pos + ImVec2(labelWidth, 0));
        
        if (ImGui::BeginChild("##video_editor_views", ImVec2(video_editor_width, clip_window_size.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 editor_view_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 editor_view_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(editor_view_window_pos, editor_view_window_pos + editor_view_window_size, COL_DARK_ONE);
            switch (VideoEditorWindowIndex)
            {
                case 0: ShowVideoBluePrintWindow(draw_list, selected_clip); break;
                case 1: ShowVideoColorWindow(draw_list); break;
                case 2: ShowVideoCropWindow(draw_list); break;
                case 3: ShowVideoRotateWindow(draw_list); break;
                default: break;
            }
        }
        ImGui::EndChild();
        ImGui::SetCursorScreenPos(clip_window_pos + ImVec2(video_editor_width + labelWidth, 0));
        if (ImGui::BeginChild("##video_view", ImVec2(video_view_width, clip_window_size.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 video_view_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 video_view_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(video_view_window_pos, video_view_window_pos + video_view_window_size, COL_DEEP_DARK);

            // Draw Video Filter Play control bar
            ImVec2 PanelBarPos = video_view_window_pos + ImVec2(0, (video_view_window_size.y - 36) / 2);
            ImVec2 PanelBarSize = ImVec2(video_view_window_size.x, 36);
            draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);
            // Preview buttons Stop button is center of Panel bar
            auto PanelCenterX = PanelBarPos.x + video_view_window_size.x / 2;
            auto PanelButtonY = PanelBarPos.y + 2;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32 - 8 - 32 - 8 - 32, PanelButtonY));
            if (ImGui::Button(ICON_TO_START "##video_filter_tostart", ImVec2(32, 32)))
            {
                if (selected_clip && !selected_clip->bPlay)
                {
                    selected_clip->mCurrent = selected_clip->mStart;
                    selected_clip->Seek();
                }
            }
            ImGui::ShowTooltipOnHover("To Start");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32 - 8 - 32, PanelButtonY));
            if (ImGui::Button(ICON_STEP_BACKWARD "##video_filter_step_backward", ImVec2(32, 32)))
            {
                if (selected_clip)
                {
                    selected_clip->bForward = false;
                    selected_clip->mCurrent -= sequencer->mFrameInterval;
                    if (selected_clip->mCurrent < selected_clip->mStart)
                        selected_clip->mCurrent = selected_clip->mStart;
                    if (sequencer)
                    {
                        sequencer->bForward = false;
                    }
                }
            }
            ImGui::ShowTooltipOnHover("Step Prev");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - 8 - 32, PanelButtonY));
            if (ImGui::Button(ICON_FAST_BACKWARD "##video_filter_reverse", ImVec2(32, 32)))
            {
                if (selected_clip)
                {
                    selected_clip->bForward = false;
                    selected_clip->bPlay = true;
                    if (sequencer)
                    {
                        sequencer->bForward = false;
                    }
                }
            }
            ImGui::ShowTooltipOnHover("Reverse");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16, PanelButtonY));
            if (ImGui::Button(ICON_STOP "##video_filter_stop", ImVec2(32, 32)))
            {
                if (selected_clip)
                {
                    selected_clip->bPlay = false;
                    selected_clip->mLastTime = -1;
                }
            }
            ImGui::ShowTooltipOnHover("Stop");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8, PanelButtonY));
            if (ImGui::Button(ICON_FAST_FORWARD "##video_filter_play", ImVec2(32, 32)))
            {
                if (selected_clip)
                {
                    selected_clip->bForward = true;
                    selected_clip->bPlay = true;
                    if (sequencer)
                    {
                        sequencer->bForward = true;
                    }
                }
            }
            ImGui::ShowTooltipOnHover("Play");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 32 + 8, PanelButtonY));
            if (ImGui::Button(ICON_STEP_FORWARD "##video_filter_step_forward", ImVec2(32, 32)))
            {
                if (selected_clip)
                {
                    selected_clip->bForward = true;
                    selected_clip->mCurrent += sequencer->mFrameInterval;
                    if (selected_clip->mCurrent > selected_clip->mEnd)
                        selected_clip->mCurrent = selected_clip->mEnd;
                    if (sequencer)
                    {
                        sequencer->bForward = true;
                    }
                }
            }
            ImGui::ShowTooltipOnHover("Step Next");

            ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 32 + 8 + 32 + 8, PanelButtonY));
            if (ImGui::Button(ICON_TO_END "##video_filter_toend", ImVec2(32, 32)))
            {
                if (selected_clip && !selected_clip->bPlay)
                {
                    selected_clip->mCurrent = selected_clip->mEnd;
                    selected_clip->Seek();
                }
            }
            ImGui::ShowTooltipOnHover("To End");
            ImGui::PopStyleColor(3);

            // filter input texture area
            ImVec2 InputVideoPos;
            ImVec2 InputVideoSize;
            InputVideoPos = video_view_window_pos + ImVec2(4, 4);
            InputVideoSize = ImVec2(video_view_window_size.x - 8, (video_view_window_size.y - PanelBarSize.y - 8) / 2);
            ImVec2 OutputVideoPos;
            ImVec2 OutputVideoSize;
            OutputVideoPos = video_view_window_pos + ImVec2(4, 4 + InputVideoSize.y + PanelBarSize.y);
            OutputVideoSize = ImVec2(video_view_window_size.x - 8, (video_view_window_size.y - PanelBarSize.y - 8) / 2);
            if (selected_clip)
            {
                std::pair<ImGui::ImMat, ImGui::ImMat> pair;
                auto ret = selected_clip->GetFrame(pair);
                if (ret)
                {
                    ImGui::ImMatToTexture(pair.first, selected_clip->mFilterInputTexture);
                    ImGui::ImMatToTexture(pair.second, selected_clip->mFilterOutputTexture);
                    if (selected_clip->bPlay)
                    {
                        if (selected_clip->bForward)
                        {
                            selected_clip->mCurrent += selected_clip->mFrameInterval;
                            if (selected_clip->mCurrent > selected_clip->mEnd)
                                selected_clip->mCurrent = selected_clip->mEnd;
                        }
                        else
                        {
                            selected_clip->mCurrent -= selected_clip->mFrameInterval;
                            if (selected_clip->mCurrent < selected_clip->mStart)
                                selected_clip->mCurrent = selected_clip->mStart;
                        }
                    }
                }
                ImGuiIO& io = ImGui::GetIO();
                float pos_x = 0, pos_y = 0;
                bool draw_compare = false;
                ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
                ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
                {
                    // filter input texture area
                    float offset_x = 0, offset_y = 0;
                    float tf_x = 0, tf_y = 0;
                    ShowVideoWindow(selected_clip->mFilterInputTexture, InputVideoPos, InputVideoSize, offset_x, offset_y, tf_x, tf_y);
                    if (ImGui::IsItemHovered() && selected_clip->mFilterInputTexture)
                    {
                        float image_width = ImGui::ImGetTextureWidth(selected_clip->mFilterInputTexture);
                        float image_height = ImGui::ImGetTextureHeight(selected_clip->mFilterInputTexture);
                        float scale_w = image_width / (InputVideoSize.x - tf_x * 2);
                        float scale_h = image_height / (InputVideoSize.y - tf_y * 2);
                        pos_x = (io.MousePos.x - offset_x) * scale_w;
                        pos_y = (io.MousePos.y - offset_y) * scale_h;
                        draw_compare = true;
                    }
                }
                {
                    // filter output texture area
                    float offset_x = 0, offset_y = 0;
                    float tf_x = 0, tf_y = 0;
                    ShowVideoWindow(selected_clip->mFilterOutputTexture, OutputVideoPos, OutputVideoSize, offset_x, offset_y, tf_x, tf_y);
                    if (ImGui::IsItemHovered() && selected_clip->mFilterOutputTexture)
                    {
                        float image_width = ImGui::ImGetTextureWidth(selected_clip->mFilterOutputTexture);
                        float image_height = ImGui::ImGetTextureHeight(selected_clip->mFilterOutputTexture);
                        float scale_w = image_width / (OutputVideoSize.x - tf_x * 2);
                        float scale_h = image_height / (OutputVideoSize.y - tf_y * 2);
                        pos_x = (io.MousePos.x - offset_x) * scale_w;
                        pos_y = (io.MousePos.y - offset_y) * scale_h;
                        draw_compare = true;
                    }
                }
                if (draw_compare)
                {
                    float region_sz = 360.0f;
                    float texture_zoom = 1.0f;
                    if (selected_clip->mFilterInputTexture)
                    {
                        float image_width = ImGui::ImGetTextureWidth(selected_clip->mFilterInputTexture);
                        float image_height = ImGui::ImGetTextureHeight(selected_clip->mFilterInputTexture);
                        float region_x = pos_x - region_sz * 0.5f;
                        float region_y = pos_y - region_sz * 0.5f;
                        if (region_x < 0.0f) { region_x = 0.0f; }
                        else if (region_x > image_width - region_sz) { region_x = image_width - region_sz; }
                        if (region_y < 0.0f) { region_y = 0.0f; }
                        else if (region_y > image_height - region_sz) { region_y = image_height - region_sz; }
                        ImGui::SetNextWindowPos(InputVideoPos - ImVec2(region_sz + 40, 0));
                        ImGui::BeginTooltip();
                        ImVec2 uv0 = ImVec2((region_x) / image_width, (region_y) / image_height);
                        ImVec2 uv1 = ImVec2((region_x + region_sz) / image_width, (region_y + region_sz) / image_height);
                        ImGui::Image(selected_clip->mFilterInputTexture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                        ImGui::EndTooltip();
                    }
                    if (selected_clip->mFilterOutputTexture)
                    {
                        float image_width = ImGui::ImGetTextureWidth(selected_clip->mFilterOutputTexture);
                        float image_height = ImGui::ImGetTextureHeight(selected_clip->mFilterOutputTexture);
                        float region_x = pos_x - region_sz * 0.5f;
                        float region_y = pos_y - region_sz * 0.5f;
                        if (region_x < 0.0f) { region_x = 0.0f; }
                        else if (region_x > image_width - region_sz) { region_x = image_width - region_sz; }
                        if (region_y < 0.0f) { region_y = 0.0f; }
                        else if (region_y > image_height - region_sz) { region_y = image_height - region_sz; }
                        ImGui::BeginTooltip();
                        ImVec2 uv0 = ImVec2((region_x) / image_width, (region_y) / image_height);
                        ImVec2 uv1 = ImVec2((region_x + region_sz) / image_width, (region_y + region_sz) / image_height);
                        ImGui::Image(selected_clip->mFilterOutputTexture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("##video_editor_timeline", ImVec2(window_size.x, clip_timeline_height), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 clip_timeline_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 clip_timeline_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(clip_timeline_window_pos, clip_timeline_window_pos + clip_timeline_window_size, COL_DARK_TWO);
        if (selected_clip)
        {
            // Draw Clip TimeLine
            ClipTimeLine(selected_clip);
        }
        else
        {
            // TODO::Dicky Draw Help("Please Select clip from Timeline")
        }
    }
    ImGui::EndChild();
}

static void ShowVideoFusionWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Video Fusion");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowAudioEditorWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Audio Editor");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowMediaAnalyseWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Analyse");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowMediaAIWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida AI");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Editor";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1680;
    property.height = 1024;
}

void Application_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
    Logger::GetDefaultLogger()->SetShowLevels(Logger::DEBUG);
    GetMediaReaderLogger()->SetShowLevels(Logger::DEBUG);
    ImGui::ResetTabLabelStyle(ImGui::ImGuiTabLabelStyle_Dark, *tab_style);
#ifdef USE_BOOKMARK
	// load bookmarks
	std::ifstream docFile(bookmark_path, std::ios::in);
	if (docFile.is_open())
	{
		std::stringstream strStream;
		strStream << docFile.rdbuf();//read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif
    sequencer = new MediaSequencer();
}

void Application_Finalize(void** handle)
{
    for (auto item : media_items) delete item;
    if (sequencer)
        delete sequencer;
#ifdef USE_BOOKMARK
	// save bookmarks
	std::ofstream configFileWriter(bookmark_path, std::ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
}

bool Application_Frame(void * handle)
{
    const float media_icon_size = 144; 
    const float tool_icon_size = 32;
    static bool show_about = false;
    static bool expanded = true;
    ImGuiFileDialogFlags fflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_DisableCreateDirectoryButton;
    const std::string ffilters = "Video files (*.mp4 *.mov *.mkv *.avi *.webm *.ts){.mp4,.mov,.mkv,.avi,.webm,.ts},Audio files (*.wav *.mp3 *.aac *.ogg *.ac3 *.dts){.wav,.mp3,.aac,.ogg,.ac3,.dts},Image files (*.png *.gif *.jpg *.jpeg *.tiff *.webp){.png,.gif,.jpg,.jpeg,.tiff,.webp},All File(*.*){.*}";
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static const int numControlPanelTabs = sizeof(ControlPanelTabNames)/sizeof(ControlPanelTabNames[0]);
    static const int numMainWindowTabs = sizeof(MainWindowTabNames)/sizeof(MainWindowTabNames[0]);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Content", nullptr, flags);
    if (show_about)
    {
        ImGui::OpenPopup("##about", ImGuiPopupFlags_AnyPopup);
    }
    if (ImGui::BeginPopupModal("##about", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Media Editor Demo(ImGui)");
        ImGui::Separator();
        ImGui::Text("  Dicky 2021");
        ImGui::Separator();
        int i = ImGui::GetCurrentWindow()->ContentSize.x;
        ImGui::Indent((i - 40.0f) * 0.5f);
        if (ImGui::Button("OK", ImVec2(40, 0))) { show_about = false; ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImVec2 window_size = ImGui::GetWindowSize();
    static float size_main_h = 0.75;
    static float size_timeline_h = 0.25;
    static float old_size_timeline_h = size_timeline_h;

    ImGui::PushID("##Main_Timeline");
    float main_height = size_main_h * window_size.y;
    float timeline_height = size_timeline_h * window_size.y;
    Splitter(false, 4.0f, &main_height, &timeline_height, 32, 32);
    size_main_h = main_height / window_size.y;
    size_timeline_h = timeline_height / window_size.y;
    ImGui::PopID();
    ImVec2 main_pos(4, 0);
    ImVec2 main_size(window_size.x, main_height + 4);
    ImGui::SetNextWindowPos(main_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Top_Panel", main_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        static int ControlPanelIndex = 0;
        static int MainWindowIndex = 0;
        ImVec2 main_window_size = ImGui::GetWindowSize();
        static float size_media_bank_w = 0.2;
        static float size_main_w = 0.8;
        ImGui::PushID("##Control_Panel_Main");
        float bank_width = size_media_bank_w * main_window_size.x;
        float main_width = size_main_w * main_window_size.x;
        Splitter(true, 4.0f, &bank_width, &main_width, media_icon_size + tool_icon_size, 96);
        size_media_bank_w = bank_width / main_window_size.x;
        size_main_w = main_width / main_window_size.x;
        ImGui::PopID();
        
        static bool bank_expanded = true;
        ImVec2 bank_pos(4, 0);
        ImVec2 bank_size(bank_width - 4, main_window_size.y - 4);
        ImGui::SetNextWindowPos(bank_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Control_Panel_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 bank_window_size = ImGui::GetWindowSize();
            ImGui::TabLabels(numControlPanelTabs, ControlPanelTabNames, ControlPanelIndex, ControlPanelTabTooltips , false, nullptr, nullptr, false, false, nullptr, nullptr);

            // make control panel area
            ImVec2 area_pos = ImVec2(tool_icon_size + 4, 32);
            ImGui::SetNextWindowPos(area_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Control_Panel_content", bank_window_size - ImVec2(tool_icon_size + 4, 32), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                auto wmin = area_pos;
                auto wmax = wmin + ImGui::GetContentRegionAvail();
                draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);
                switch (ControlPanelIndex)
                {
                    case 0: ShowMediaBankWindow(draw_list, media_icon_size); break;
                    case 1: ShowTransitionBankWindow(draw_list); break;
                    case 2: ShowFilterBankWindow(draw_list); break;
                    case 3: ShowMediaOutputWindow(draw_list); break;
                    default: break;
                }
            }
            ImGui::EndChild();

            // add tool bar
            ImGui::SetCursorPos(ImVec2(0,32));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
            if (ImGui::Button(ICON_IGFD_ADD "##AddMedia", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Open Media Source
                ImGuiFileDialog::Instance()->OpenModal("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Media File", 
                                                        ffilters.c_str(),
                                                        ".",
                                                        1, 
                                                        IGFDUserDatas("Media Source"), 
                                                        fflags);
            }
            ImGui::ShowTooltipOnHover("Add new media into bank");
            if (ImGui::Button(ICON_FA_WHMCS "##Configure", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show Setting
            }
            ImGui::ShowTooltipOnHover("Configure");
            if (ImGui::Button(ICON_FA5_INFO_CIRCLE "##About", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show About
                show_about = true;
            }
            ImGui::ShowTooltipOnHover("About Media Editor");
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();

        ImVec2 main_sub_pos(bank_width + 8, 0);
        ImVec2 main_sub_size(main_width - 8, main_window_size.y - 4);
        ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Main_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            // full background
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            //ImVec2 main_window_size = ImGui::GetWindowSize();
            ImGui::TabLabels(numMainWindowTabs, MainWindowTabNames, MainWindowIndex, MainWindowTabTooltips , false, nullptr, nullptr, false, false, nullptr, nullptr);
            auto wmin = main_sub_pos + ImVec2(0, 32);
            auto wmax = wmin + ImGui::GetContentRegionAvail() - ImVec2(8, 0);
            draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);
            if (ImGui::BeginChild("##Main_Window_content", wmax - wmin, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                switch (MainWindowIndex)
                {
                    case 0: ShowMediaPreviewWindow(draw_list); break;
                    case 1: ShowVideoEditorWindow(draw_list); break;
                    case 2: ShowVideoFusionWindow(draw_list); break;
                    case 3: ShowAudioEditorWindow(draw_list); break;
                    case 4: ShowMediaAnalyseWindow(draw_list); break;
                    case 5: ShowMediaAIWindow(draw_list); break;
                    default: break;
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    
    ImVec2 panel_pos(4, size_main_h * window_size.y + 12);
    ImVec2 panel_size(window_size.x - 4, size_timeline_h * window_size.y - 12);
    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    bool _expanded = expanded;
    if (ImGui::BeginChild("##Sequencor", panel_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImSequencer::Sequencer(sequencer, &_expanded, 
                                ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_TIME | ImSequencer::SEQUENCER_DEL |
                                ImSequencer::SEQUENCER_LOCK | ImSequencer::SEQUENCER_VIEW | ImSequencer::SEQUENCER_MUTE | ImSequencer::SEQUENCER_RESTORE);
        
        if (expanded != _expanded)
        {
            if (!_expanded)
            {
                old_size_timeline_h = size_timeline_h;
                size_timeline_h = 40.0f / window_size.y;
                size_main_h = 1 - size_timeline_h;
            }
            else
            {
                size_timeline_h = old_size_timeline_h;
                size_main_h = 1.0f - size_timeline_h;
            }
            expanded = _expanded;
        }
    }
    ImGui::EndChild();
    
    ImGui::PopStyleColor();
    ImGui::End();

    // File Dialog
    ImVec2 minSize = ImVec2(0, 300);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (ImGuiFileDialog::Instance()->Display("##MediaEditFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("Media Source") == 0)
            {
                auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                auto file_surfix = ImGuiFileDialog::Instance()->GetCurrentFileSurfix();
                int type = SEQUENCER_ITEM_UNKNOWN;
                if (!file_surfix.empty())
                {
                    if ((file_surfix.compare(".mp4") == 0) ||
                        (file_surfix.compare(".mov") == 0) ||
                        (file_surfix.compare(".mkv") == 0) ||
                        (file_surfix.compare(".avi") == 0) ||
                        (file_surfix.compare(".webm") == 0) ||
                        (file_surfix.compare(".ts") == 0))
                        type = SEQUENCER_ITEM_VIDEO;
                    else 
                        if ((file_surfix.compare(".wav") == 0) ||
                            (file_surfix.compare(".mp3") == 0) ||
                            (file_surfix.compare(".aac") == 0) ||
                            (file_surfix.compare(".ac3") == 0) ||
                            (file_surfix.compare(".dts") == 0) ||
                            (file_surfix.compare(".ogg") == 0))
                        type = SEQUENCER_ITEM_AUDIO;
                    else 
                        if ((file_surfix.compare(".jpg") == 0) ||
                            (file_surfix.compare(".jpeg") == 0) ||
                            (file_surfix.compare(".png") == 0) ||
                            (file_surfix.compare(".gif") == 0) ||
                            (file_surfix.compare(".tiff") == 0) ||
                            (file_surfix.compare(".webp") == 0))
                        type = SEQUENCER_ITEM_PICTURE;
                }
                MediaItem * item = new MediaItem(file_name, file_path, type);
                media_items.push_back(item);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    return false;
}