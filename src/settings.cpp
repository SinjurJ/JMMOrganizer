module;

#include <Button.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Rect.h>
#include <TextView.h>
#include <Window.h>

export module settings;

export class SettingsWindow : public BWindow {
  public:
    SettingsWindow()
        : BWindow(BRect(0, 0, 400, 400), "Settings", B_TITLED_WINDOW,
                  B_NOT_RESIZABLE | B_NOT_ZOOMABLE) {
        BLayoutBuilder::Grid<>(this)
            .Add(source_view, 0, 0, 3, 1)
            .Add(new BButton("Browse" B_UTF8_ELLIPSIS, nullptr), 4, 0)
            .Add(destination_view, 0, 1, 3, 1)
            .Add(new BButton("Browse" B_UTF8_ELLIPSIS, nullptr), 4, 1);
    }

  private:
    BTextView *source_view = new BTextView("Source");
    BTextView *destination_view = new BTextView("Destination");
};