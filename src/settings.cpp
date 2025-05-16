module;

#include <Application.h>
#include <Button.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Rect.h>
#include <SeparatorView.h>
#include <TextControl.h>
#include <Window.h>

export module settings;

import utilities;

export class SettingsWindow : public BWindow {
  public:
    SettingsWindow()
        : BWindow(BRect(0, 0, 0, 0), "Settings", B_TITLED_WINDOW,
                  B_NOT_RESIZABLE | B_NOT_ZOOMABLE) {
        BLayoutBuilder::Grid<>(this)
            .Add(source_control->CreateLabelLayoutItem(), 0, 0)
            .Add(source_control->CreateTextViewLayoutItem(), 1, 0, 3, 1)
            .Add(new BButton("Browse" B_UTF8_ELLIPSIS, nullptr), 4, 0)
            .Add(destination_control->CreateLabelLayoutItem(), 0, 1)
            .Add(destination_control->CreateTextViewLayoutItem(), 1, 1, 3, 1)
            .Add(new BButton("Browse" B_UTF8_ELLIPSIS, nullptr), 4, 1)
            .Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER), 0, 2, 5, 1)
            .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 0, 3, 5, 1)
            .Add(new BButton("Cancel", nullptr))
            .AddGlue()
            .Add(new BButton("Apply", nullptr));
    }

    void Quit() override {
        BMessage message(SETTINGS_CLOSED);
        be_app->PostMessage(&message);

        BWindow::Quit();
    }

  private:
    // TODO determine if these need to be deleted
    BTextControl *source_control =
        new BTextControl("Source folder:", "/home/music/", nullptr);
    BTextControl *destination_control =
        new BTextControl("Destination folder:", "/home/music/", nullptr);
};