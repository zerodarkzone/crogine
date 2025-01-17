/*********************************************************************
(c) Matt Marchant 2021
http://trederia.blogspot.com

Zlib license.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.
*********************************************************************/

#include "EditorWindow.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Keyboard.hpp>

#include <fstream>
#include <iostream>

namespace
{

}

EditorWindow::EditorWindow()
{
    m_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
}

//public
void EditorWindow::update()
{
    //based on https://github.com/BalazsJako/ColorTextEditorDemo 
    if (visible)
    {
        auto cpos = m_editor.GetCursorPosition();
        ImGui::Begin("Text Editor", &visible, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
        ImGui::SetWindowSize(ImVec2(640.f, 480.f), ImGuiCond_FirstUseEver);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                {
                    ImGui::OpenPopup("?New?");

                    //TODO remove this when the confirmation box works
                    m_editor.SetText("");
                    m_currentFile = {};
                }
                //if anyone knows why this doesn't work (even though it's copied from the demo)
                //please halp...
                if (ImGui::BeginPopupModal("?New?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Unsaved Work Will Be Lost");
                    ImGui::Separator();

                    if (ImGui::Button("OK", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                        m_editor.SetText("");
                        m_currentFile = {};
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::MenuItem("Open", "Ctrl+O"))
                {
                    open();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    if (!m_currentFile.empty())
                    {
                        save(m_currentFile);
                    }
                    else
                    {
                        saveAs();
                    }
                }
                if (ImGui::MenuItem("Save As"))
                {
                    saveAs();
                }
                if (ImGui::MenuItem("Close"))
                {
                    close();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                bool ro = m_editor.IsReadOnly();
                if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                {
                    m_editor.SetReadOnly(ro);
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, !ro && m_editor.CanUndo()))
                {
                    m_editor.Undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !ro && m_editor.CanRedo()))
                {
                    m_editor.Redo();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, m_editor.HasSelection()))
                {
                    m_editor.Copy();
                }
                if (ImGui::MenuItem("Cut", "Ctrl+X", nullptr, !ro && m_editor.HasSelection()))
                {
                    m_editor.Cut();
                }
                if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && m_editor.HasSelection()))
                {
                    m_editor.Delete();
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                {
                    m_editor.Paste();
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Select all", "Ctrl+A", nullptr))
                {
                    m_editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(m_editor.GetTotalLines(), 0));
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Dark palette"))
                {
                    m_editor.SetPalette(TextEditor::GetDarkPalette());
                }
                if (ImGui::MenuItem("Light palette"))
                {
                    m_editor.SetPalette(TextEditor::GetLightPalette());
                }
                if (ImGui::MenuItem("Retro blue palette"))
                {
                    m_editor.SetPalette(TextEditor::GetRetroBluePalette());
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }


        ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, m_editor.GetTotalLines(),
            m_editor.IsOverwrite() ? "Ovr" : "Ins",
            m_editor.CanUndo() ? "*" : " ",
            m_editor.GetLanguageDefinition().mName.c_str(), m_currentFile.c_str());

        //done before Render() - below - because it resets the editor's
        //internal flag.
        if (m_editor.IsTextChanged())
        {

        }

        m_editor.Render("TextEditor");
        ImGui::End();

        if (!visible)
        {
            //window was closed
            close();
        }

        doHotkeys();
    }
}

//private
void EditorWindow::open()
{
    const auto result = cro::FileSystem::openFileDialogue();
    if (!result.empty())
    {
        std::ifstream file(result);
        if (file.is_open() && file.good())
        {
            std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            m_editor.SetText(str);

            m_currentFile = result;
        }
    }
}

void EditorWindow::save(const std::string& path)
{
    auto textToSave = m_editor.GetText();
    std::ofstream file(path);
    if (file.is_open() && file.good())
    {
        file.write(textToSave.c_str(), textToSave.size());
        m_currentFile = path;
    }
}

void EditorWindow::saveAs()
{
    const auto result = cro::FileSystem::saveFileDialogue();
    if (!result.empty())
    {
        save(result);
    }
}

void EditorWindow::close() 
{
    if (cro::FileSystem::showMessageBox("Are You Sure?", "Do you want to save the current file?", cro::FileSystem::YesNo))
    {
        if (m_currentFile.empty())
        {
            saveAs();
        }
        else
        {
            save(m_currentFile);
        }
    }

    m_currentFile.clear();
    m_editor.SetText("");
}

void EditorWindow::doHotkeys()
{
    ImGuiIO& io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ctrl && !shift && !alt && cro::Keyboard::isKeyPressed(SDLK_o))
    {
        open();
    }
    else if (ctrl && !shift && !alt && cro::Keyboard::isKeyPressed(SDLK_s))
    {
        if (!m_currentFile.empty())
        {
            save(m_currentFile);
        }
        else
        {
            saveAs();
        }
    }
}