#include <AboutWindow.h>
#include <AppDefs.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <OS.h>
#include <Query.h>
#include <Rect.h>
#include <ScrollView.h>
#include <SeparatorItem.h>
#include <StackOrHeapArray.h>
#include <String.h>
#include <TextView.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Window.h>

import track_processing;

const BString APPLICATION_NAME = "JMMOrganizer"; // TODO determine if efficient

class JMMOrganizerWindow : public BWindow {
  public:
    JMMOrganizerWindow(const char *title)
        : BWindow(BRect(50, 50, 600, 100), title, B_TITLED_WINDOW,
                  B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE |
                      B_QUIT_ON_WINDOW_CLOSE) {

        // TODO determine what needs to be deleted explicitly

        BMenuBar *key_menu_bar = new BMenuBar("Key Menu Bar");
        BMenu *first_key_menu = new BMenu("File");
        key_menu_bar->AddItem(first_key_menu);

        BMenuItem *about_menu_item =
            new BMenuItem(BString("About").Append(B_UTF8_ELLIPSIS),
                          new BMessage(B_ABOUT_REQUESTED));
        BMenuItem *settings_menu_item =
            new BMenuItem(BString("Settings").Append(B_UTF8_ELLIPSIS), nullptr);
        BMenuItem *quit_menu_item =
            new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED));
        quit_menu_item->SetShortcut('Q', B_COMMAND_KEY);
        first_key_menu->AddItem(about_menu_item);
        first_key_menu->AddItem(settings_menu_item);
        first_key_menu->AddItem(new BSeparatorItem());
        first_key_menu->AddItem(quit_menu_item);

        artists_check_box->SetEnabled(false);
        genres_check_box->SetEnabled(false);
        tracks_check_box->SetEnabled(false);

        generate_button->SetEnabled(false);

        progress_view->MakeEditable(false);
        progress_view->MakeSelectable(false);

        progress_view->AdoptSystemColors();

        BScrollView *progress_scroll_view =
            new BScrollView("Progress Scroll", progress_view, 0, false, true);

        // progress_view->SetResizingMode(B_FOLLOW_ALL_SIDES);

        BLayoutBuilder::Grid<>(this)
            .Add(key_menu_bar, 0, 0, 2, 1)
            .Add(albums_check_box, 0, 1)
            .Add(artists_check_box, 0, 2)
            .Add(genres_check_box, 0, 3)
            .Add(tracks_check_box, 0, 4)
            .Add(generate_button, 0, 5)
            .Add(progress_scroll_view, 1, 1, 1, 5);
    }

    void MessageReceived(BMessage *message) {
        switch (message->what) {
        case ACTIVATE_ALBUMS:
        case ACTIVATE_ARTISTS:
        case ACTIVATE_GENRES:
        case ACTIVATE_TRACKS:
            generate_button->SetEnabled(
                albums_check_box->Value() || artists_check_box->Value() ||
                genres_check_box->Value() || tracks_check_box->Value());
            break;
        case FINISHED_PROCESS:
            process_tracks_thread = 0;
            music_query.Clear();
            generate_button->SetEnabled(
                albums_check_box->Value() || artists_check_box->Value() ||
                genres_check_box->Value() || tracks_check_box->Value());
            break;
        case GENERATE:
            generate_button->SetEnabled(false);
            [&] { // TODO don't use lambda
                if (!albums_check_box->Value()) {
                    return;
                }

                if (process_tracks_thread > 0) {
                    return;
                }

                // TODO set predicate and volume in better location
                // TODO should the query stuff be in JMMOrganizerApplication?
                BVolume boot_volume; // TODO possibly implement other volumes
                BVolumeRoster volume_roster;
                volume_roster.GetBootVolume(&boot_volume);
                music_query.SetVolume(&boot_volume);
                music_query.SetPredicate("BEOS:TYPE == audio/* && name == *");
                if (music_query.Fetch() != B_OK) {
                    // TODO show window clarifying issue
                    return;
                }

                process_tracks_thread = spawn_thread(
                    processTracks, "process_tracks", B_LOW_PRIORITY,
                    new ProcessTracksData(&music_query, source_path,
                                          destination_path, storeAlbum,
                                          generateAlbumsAndSingles, this));
                if (process_tracks_thread > 0) {
                    resume_thread(process_tracks_thread);
                }
                return;
            }();
            break;
        case LINE_FROM_PROCESS:
            [&] {
                BString new_line;
                message->FindString("line", &new_line);
                progress_view->Insert(0, new_line, new_line.Length());
                return;
            }();
            break;
        default:
            be_app->PostMessage(message);
        }
    }

    bool QuitRequested() {
        // TODO ensure there aren't major memory leaks
        // TODO consider something better than killing the thread
        kill_thread(process_tracks_thread);
        return true;
    }

  private:
    // TODO should the pointers be const?
    // TODO should there be separate bools stored for state?
    BCheckBox *albums_check_box =
        new BCheckBox("Albums", new BMessage(ACTIVATE_ALBUMS));
    BCheckBox *artists_check_box =
        new BCheckBox("Artists", new BMessage(ACTIVATE_ARTISTS));
    BCheckBox *genres_check_box =
        new BCheckBox("Genres", new BMessage(ACTIVATE_GENRES));
    BCheckBox *tracks_check_box =
        new BCheckBox("Tracks", new BMessage(ACTIVATE_TRACKS));

    BButton *generate_button = new BButton("Generate", new BMessage(GENERATE));

    BTextView *progress_view = new BTextView("Progress");

    BQuery music_query; // TODO should this be in JMMOrganizerApplication?
    BString source_path = "/boot/home/music/";
    BString destination_path = "/boot/home/music/";

    int process_tracks_thread = 0;
};

class JMMOrganizerApplication : public BApplication {
  public:
    JMMOrganizerApplication(const char *signature) : BApplication(signature) {
        window->Show();
    }

    void AboutRequested() {
        BApplication::AboutRequested();
        BAboutWindow *about_window =
            new BAboutWindow(APPLICATION_NAME, "signature");

        // TODO this might be silly over a standard char **
        BStackOrHeapArray<const char *, 2> authors(2);
        authors[0] = "Jareth McGhee";
        authors[1] = nullptr;

        about_window->AddDescription(
            "JMMOrganizer is an easy way to take a disorganized music library "
            "and organize it.");
        about_window->AddAuthors(authors);
        about_window->AddCopyright(2025, "Jareth McGhee");
        about_window->AddExtraInfo(
            "JMMOrganizer is provided under the MIT No Attribution license. "
            "More information about the program and third-party licenses can "
            "be found at the source code repository.\n\n"
            "GitHub:\n"
            "\thttps://github.com/SinjurJ/JMMOrganizer\n"
            "IPFS: PLACEHOLDER ADDRESS\n" // TODO create real address
            "\tipns://"
            "k51qzi5uqu5dh6ombngx7bhdw9ivzioibwig5qdqzpabl3xmm3h61nnre2arfy"
            "/JMMOrganizer");

        about_window->Show();
    }

  private:
    JMMOrganizerWindow *window = new JMMOrganizerWindow(APPLICATION_NAME);
};

int main() {
    // TODO should it be SinjurJ instead of sinjurj?
    JMMOrganizerApplication application(
        BString("application/x-nd.sinjurj-").Append(APPLICATION_NAME));
    application.Run();
}
