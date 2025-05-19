module;

#include <Application.h>
#include <Button.h>
#include <Errors.h>
#include <FilePanel.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Rect.h>
#include <SeparatorView.h>
#include <StorageDefs.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

#define DEFAULT_SOURCE "/boot/home/music"
#define DEFAULT_DESTINATION "/boot/home/music"

export module settings;

import utilities;

BString extractPathFromRefMessage(const BMessage &message) {
    entry_ref file_ref;
    if (message.FindRef("refs", &file_ref) != B_OK) {
        return BString();
    }
    return BPath(&file_ref).Path();
}

export class SettingsWindow : public BWindow {
  public:
    SettingsWindow(const BMessage &settings = BMessage())
        : BWindow(BRect(0, 0, 0, 0), "Settings", B_TITLED_WINDOW,
                  B_NOT_RESIZABLE | B_NOT_ZOOMABLE) {
        // TODO maybe use struct instead of message?
        current_settings = new BMessage(SETTINGS);
        current_settings->AddString(
            "source", settings.GetString("source", DEFAULT_SOURCE));
        current_settings->AddString(
            "destination",
            settings.GetString("destination", DEFAULT_DESTINATION));

        settings_messenger = new BMessenger(nullptr, this);

        BMessage source_panel_message(SETTINGS_REFS_SOURCE);
        source_panel =
            new BFilePanel(B_OPEN_PANEL, settings_messenger, nullptr,
                           B_DIRECTORY_NODE, false, &source_panel_message);
        BMessage destination_panel_message(SETTINGS_REFS_DESTINATION);
        destination_panel =
            new BFilePanel(B_OPEN_PANEL, settings_messenger, nullptr,
                           B_DIRECTORY_NODE, false, &destination_panel_message);

        source_control->SetModificationMessage(new BMessage(SETTINGS_MODIFIED));
        destination_control->SetModificationMessage(
            new BMessage(SETTINGS_MODIFIED));

        RevertSettings();

        BButton *browse_source = new BButton(
            "Browse" B_UTF8_ELLIPSIS, new BMessage(SETTINGS_BROWSE_SOURCE));
        BButton *browse_destination =
            new BButton("Browse" B_UTF8_ELLIPSIS,
                        new BMessage(SETTINGS_BROWSE_DESTINATION));

        BLayoutBuilder::Grid<>(this)
            //.SetSpacing(0, 0)
            .AddTextControl(source_control, 0, 0, B_ALIGN_LEFT, 1, 3, 1)
            .Add(browse_source, 4, 0)
            .AddTextControl(destination_control, 0, 1, B_ALIGN_LEFT, 1, 3, 1)
            .Add(browse_destination, 4, 1)
            .Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER), 0, 2, 5, 1)
            .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 0, 3, 5, 1)
            .Add(revert_button)
            .AddGlue()
            .Add(apply_button);
    }

    virtual ~SettingsWindow() {
        delete destination_panel;
        delete source_panel;
        delete settings_messenger;
    }

    void MessageReceived(BMessage *message) override {
        switch (message->what) {
        case SETTINGS_APPLY: {
            revert_button->SetEnabled(false);
            apply_button->SetEnabled(false);
            message->SetString("source",
                               BString(source_control->Text()).IsEmpty()
                                   ? DEFAULT_SOURCE
                                   : source_control->Text());
            message->SetString("destination",
                               BString(destination_control->Text()).IsEmpty()
                                   ? DEFAULT_DESTINATION
                                   : destination_control->Text());
            be_app->PostMessage(message);
            break;
        }
        case SETTINGS_BROWSE_DESTINATION: {
            source_panel->Hide();
            destination_panel->Show();
            break;
        }
        case SETTINGS_BROWSE_SOURCE:
            destination_panel->Hide();
            source_panel->Show();
            break;
        case SETTINGS_MODIFIED:
            revert_button->SetEnabled(true);
            apply_button->SetEnabled(true);
            break;
        case SETTINGS_REFS_DESTINATION: { // TODO accept drag-and-drop files
            BString file_path = extractPathFromRefMessage(*message);
            if (file_path.IsEmpty()) {
                break;
            }
            destination_control->SetText(file_path);
            break;
        }
        case SETTINGS_REFS_SOURCE: {
            BString file_path = extractPathFromRefMessage(*message);
            if (file_path.IsEmpty()) {
                break;
            }
            source_control->SetText(file_path);
            break;
        }
        case SETTINGS_REVERT:
            RevertSettings();
            break;
        default:
            be_app->PostMessage(message);
        }
    }

    void Quit() override {
        BMessage message(SETTINGS_CLOSED);
        be_app->PostMessage(&message);

        BWindow::Quit();
    }

    void RevertSettings() {
        revert_button->SetEnabled(false);
        apply_button->SetEnabled(false);

        source_control->SetText(
            current_settings->GetString("source", DEFAULT_SOURCE));
        destination_control->SetText(
            current_settings->GetString("destination", DEFAULT_DESTINATION));
    }

  private:
    BMessage *current_settings = nullptr;
    BMessenger *settings_messenger = nullptr;

    BFilePanel *source_panel = nullptr;
    BFilePanel *destination_panel = nullptr;

    // TODO determine if these need to be deleted
    BTextControl *source_control =
        new BTextControl("Source folder:", "", nullptr);
    BTextControl *destination_control =
        new BTextControl("Destination folder:", "", nullptr);

    BButton *revert_button =
        new BButton("Revert", new BMessage(SETTINGS_REVERT));
    BButton *apply_button = new BButton("Apply", new BMessage(SETTINGS_APPLY));
};