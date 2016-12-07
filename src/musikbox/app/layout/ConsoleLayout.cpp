//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ConsoleLayout.h"

#include <cursespp/Screen.h>
#include <cursespp/IMessage.h>

#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>

#include <app/util/Hotkeys.h>
#include <app/util/Version.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

template <class T>
bool tostr(T& t, const std::string& s) {
    std::istringstream iss(s);
    return !(iss >> t).fail();
}

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::box;
using namespace cursespp;

ConsoleLayout::ConsoleLayout(ITransport& transport, LibraryPtr library)
: LayoutBase()
, transport(transport)
, library(library) {
    this->SetFrameVisible(false);

    this->logs.reset(new LogWindow(this));
    this->output.reset(new OutputWindow(this));
    this->commands.reset(new cursespp::TextInput());

    this->commands->SetFocusOrder(0);
    this->output->SetFocusOrder(1);
    this->logs->SetFocusOrder(2);

    this->AddWindow(this->commands);
    this->AddWindow(this->logs);
    this->AddWindow(this->output);

    this->commands->EnterPressed.connect(this, &ConsoleLayout::OnEnterPressed);

    this->Help();
}

ConsoleLayout::~ConsoleLayout() {

}

void ConsoleLayout::OnLayout() {
    const int cx = this->GetWidth();
    const int cy = this->GetHeight();
    const int x = this->GetX();
    const int y = this->GetY();

    const int leftCx = cx / 2;
    const int rightCx = cx - leftCx;

    /* top left */
    this->output->MoveAndResize(x, y, leftCx, cy - 3);

    /* bottom left */
    this->commands->MoveAndResize(x, cy - 3, leftCx, 3);

    /* right */
    this->logs->MoveAndResize(cx / 2, 0, rightCx, cy);
}

void ConsoleLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), "console");
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), "library");
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), "settings");
        shortcuts->AddShortcut("^D", "quit");
        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateConsole));
    }
}

void ConsoleLayout::OnEnterPressed(TextInput *input) {
    std::string command = this->commands->GetText();
    this->commands->SetText("");

    output->WriteLine("> " + command + "\n", COLOR_PAIR(CURSESPP_TEXT_DEFAULT));

    if (!this->ProcessCommand(command)) {
        if (command.size()) {
            output->WriteLine(
                "illegal command: '" +
                command +
                "'\n", COLOR_PAIR(CURSESPP_TEXT_ERROR));
        }
    }
}


void ConsoleLayout::Seek(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        double newPosition = 0;
        if (tostr<double>(newPosition, args[0])) {
            transport.SetPosition(newPosition);
        }
    }
}

void ConsoleLayout::SetVolume(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        float newVolume = 0;
        if (tostr<float>(newVolume, args[0])) {
            this->SetVolume(newVolume / 100.0f);
        }
    }
}

void ConsoleLayout::SetVolume(float volume) {
    transport.SetVolume(volume);
}

void ConsoleLayout::Help() {
    int64 s = -1;

    this->output->WriteLine("help:\n", s);
    this->output->WriteLine("  <tab> to switch between windows", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  addir <dir>: add a music directory", s);
    this->output->WriteLine("  rmdir <dir>: remove a music directory", s);
    this->output->WriteLine("  lsdirs: list scanned directories", s);
    this->output->WriteLine("  rescan: rescan paths for new metadata", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  play <uri>: play audio at <uri>", s);
    this->output->WriteLine("  pause: pause/resume", s);
    this->output->WriteLine("  stop: stop and clean up everything", s);
    this->output->WriteLine("  volume: <0 - 100>: set % volume", s);
    this->output->WriteLine("  clear: clear the log window", s);
    this->output->WriteLine("  seek <seconds>: seek to <seconds> into track", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  plugins: list loaded plugins", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  version: show musikbox app version", s);
    this->output->WriteLine("", s);
    this->output->WriteLine("  <ctrl+d>: quit\n", s);
}

bool ConsoleLayout::ProcessCommand(const std::string& cmd) {
    std::vector<std::string> args;
    boost::algorithm::split(args, cmd, boost::is_any_of(" "));

    auto it = args.begin();
    while (it != args.end()) {
        std::string trimmed = boost::algorithm::trim_copy(*it);
        if (trimmed.size()) {
            *it = trimmed;
            ++it;
        }
        else {
            it = args.erase(it);
        }
    }

    if (args.size() == 0) {
        return true;
    }

    std::string name = args.size() > 0 ? args[0] : "";
    args.erase(args.begin());

    if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "clear") {
        this->logs->ClearContents();
    }
    else if (name == "version") {
        const std::string v = boost::str(boost::format("v%s") % VERSION);
        this->output->WriteLine(v, -1);
    }
    else if (name == "play" || name == "pl" || name == "p") {
        return this->PlayFile(args);
    }
    else if (name == "addir") {
        std::string path = boost::algorithm::join(args, " ");
        library->Indexer()->AddPath(path);
    }
    else if (name == "rmdir") {
        std::string path = boost::algorithm::join(args, " ");
        library->Indexer()->RemovePath(path);
    }
    else if (name == "lsdirs") {
        std::vector<std::string> paths;
        library->Indexer()->GetPaths(paths);
        this->output->WriteLine("paths:");
        for (size_t i = 0; i < paths.size(); i++) {
            this->output->WriteLine("  " + paths.at(i));
        }
        this->output->WriteLine("");
    }
    else if (name == "rescan" || name == "scan" || name == "index") {
        library->Indexer()->Synchronize();
    }
    else if (name == "h" || name == "help") {
        this->Help();
    }
    else if (name == "pa" || name == "pause") {
        this->Pause();
    }
    else if (name == "s" || name =="stop") {
        this->Stop();
    }
    else if (name == "sk" || name == "seek") {
        this->Seek(args);
    }
    else if (name == "plugins") {
        this->ListPlugins();
    }
    else if (name == "v" || name == "volume") {
        this->SetVolume(args);
    }
    else {
        return false;
    }

    return true;
}

bool ConsoleLayout::PlayFile(const std::vector<std::string>& args) {
    if (args.size() > 0) {
        std::string filename = boost::algorithm::join(args, " ");
        transport.Start(filename);
        return true;
    }

    return false;
}

void ConsoleLayout::Pause() {
    int state = this->transport.GetPlaybackState();
    if (state == PlaybackPaused) {
        this->transport.Resume();
    }
    else if (state == PlaybackPlaying) {
        this->transport.Pause();
    }
}

void ConsoleLayout::Stop() {
    this->transport.Stop();
}

void ConsoleLayout::ListPlugins() const {
    using musik::core::sdk::IPlugin;
    using musik::core::PluginFactory;

    typedef std::vector<std::shared_ptr<IPlugin> > PluginList;
    typedef PluginFactory::NullDeleter<IPlugin> Deleter;

    PluginList plugins = PluginFactory::Instance()
        .QueryInterface<IPlugin, Deleter>("GetPlugin");

    PluginList::iterator it = plugins.begin();
    for (; it != plugins.end(); it++) {
        std::string format =
            "    " + std::string((*it)->Name()) + " "
            "v" + std::string((*it)->Version()) + "\n"
            "    by " + std::string((*it)->Author()) + "\n";

        this->output->WriteLine(format, COLOR_PAIR(CURSESPP_TEXT_DEFAULT));
    }
}
