#include <AboutWindow.h>
#include <AppDefs.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <File.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <OS.h>
#include <Point.h>
#include <Query.h>
#include <Rect.h>
#include <ScrollView.h>
#include <SeparatorItem.h>
#include <StackOrHeapArray.h>
#include <StorageDefs.h>
#include <String.h>
#include <TextView.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Window.h>

import track_processing;

const BString APPLICATION_NAME = "JMMOrganizer"; // TODO determine if efficient

class JMMOrganizerWindow : public BWindow {
  public:
    JMMOrganizerWindow(BRect frame, const char *title)
        : BWindow(frame, title, B_TITLED_WINDOW,
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
        progress_view->MakeSelectable(true);

        BScrollView *progress_scroll_view =
            new BScrollView("Progress Scroll", progress_view, 0, false, true);

        BLayoutBuilder::Grid<>(this)
            .SetSpacing(0, 0)
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
                uint8 flags = 0;
                if (albums_check_box->Value()) {
                    flags += ALBUMS;
                }
                if (artists_check_box->Value()) {
                    flags += ARTISTS;
                }
                if (genres_check_box->Value()) {
                    flags += GENRES;
                }
                if (tracks_check_box->Value()) {
                    flags += TRACKS;
                }
                process_tracks_thread = spawn_thread(
                    processTracks, "process_tracks", B_LOW_PRIORITY,
                    new ProcessTracksData(&music_query, source_path,
                                          destination_path, flags, this));
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

        // TODO don't fail silently
        BRect current_frame = this->Frame();
        BMessage frame_message;
        if (frame_message.AddRect("frame", current_frame) != B_OK) {
            return true;
        }
        BDirectory settings_directory(
            "/boot/home/config/settings/"); // TODO get from env variables
        BFile config_file; // TODO consider opening directly with B_CREATE_FILE
        if (settings_directory.CreateFile("./JMMOrganizer", &config_file,
                                          false) != B_OK) {
            return true;
        }
        if (config_file.SetSize(frame_message.FlattenedSize()) != B_OK) {
            return true;
        }
        frame_message.Flatten(&config_file);

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
        // TODO ensure window is deleted properly on quit
        JMMOrganizerWindow *window;

        BFile config_file;
        BMessage frame_message;
        BRect frame(50, 50, 600, 100);
        if (config_file.SetTo("/boot/home/config/settings/JMMOrganizer",
                              B_READ_ONLY) != B_OK) {
            goto show_window;
        }
        if (frame_message.Unflatten(&config_file) != B_OK) {
            goto show_window;
        }
        if (frame_message.FindRect("frame", &frame) != B_OK) {
            goto show_window;
        }
    show_window:; // TODO don't use goto
        frame.SetRightTop(frame.LeftTop() + BPoint(550, 0));
        if (frame.RightBottom().y - frame.RightTop().y > 550) {
            frame.SetRightBottom(frame.RightTop() + BPoint(0, 50));
        }
        window = new JMMOrganizerWindow(frame, APPLICATION_NAME);
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

        about_window->AddDescription("JMMOrganizer is an easy way to take "
                                     "a disorganized music library "
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
};

int main() {
    // TODO should it be SinjurJ instead of sinjurj?
    JMMOrganizerApplication application(
        BString("application/x-nd.sinjurj-").Append(APPLICATION_NAME));
    application.Run();
}
