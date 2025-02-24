module;

// TODO avoid excessive use of standard library
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <tuple>

#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <File.h>
#include <Looper.h>
#include <Node.h>
#include <ObjectList.h>
#include <Path.h>
#include <Query.h>
#include <StackOrHeapArray.h>
#include <String.h>
#include <StringList.h>
#include <SupportDefs.h>
#include <SymLink.h>
#include <TypeConstants.h>
#include <fs_attr.h>

export module track_processing;

import utilities;

void copyAttributes(BNode *source, BNode *destination,
                    const BObjectList<BString, true> &include,
                    const BObjectList<BString, true> &exclude) {
    // TODO ensure B_ATTR_NAME_LENGTH leaves room for a terminating null
    BStackOrHeapArray<char, B_ATTR_NAME_LENGTH> attr_name = {0};
    attr_info info = {0, 0};
    BStackOrHeapArray<uchar, B_ATTR_NAME_LENGTH> buffer = {0};

    while (source->GetNextAttrName(attr_name) == B_OK) {
        if (include.EachElement(
                [](const BString *i, void *params) -> const BString * {
                    BStackOrHeapArray<char, B_ATTR_NAME_LENGTH> &attr_name =
                        *static_cast<
                            BStackOrHeapArray<char, B_ATTR_NAME_LENGTH> *>(
                            params);
                    if (std::regex_match(&attr_name[0], std::regex(*i))) {
                        return i;
                    }
                    return nullptr;
                },
                &attr_name) == nullptr) {
            continue;
        }
        if (exclude.EachElement(
                [](const BString *i, void *params) -> const BString * {
                    BStackOrHeapArray<char, B_ATTR_NAME_LENGTH> &attr_name =
                        *static_cast<
                            BStackOrHeapArray<char, B_ATTR_NAME_LENGTH> *>(
                            params);
                    if (std::regex_match(&attr_name[0], std::regex(*i))) {
                        return i;
                    }
                    return nullptr;
                },
                &attr_name) != nullptr) {
            continue;
        }
        source->GetAttrInfo(attr_name, &info);
        source->ReadAttr(attr_name, info.type, 0, &buffer, info.size);
        destination->WriteAttr(attr_name, info.type, 0, &buffer, info.size);
    }
}

BString storeFromAttribute(
    BString attribute, BEntry entry,
    BObjectList<std::tuple<BString, uint32, BString>, true> *storage) {
    BNode track_node = BNode(&entry);
    BString attribute_data;
    track_node.ReadAttrString(attribute, &attribute_data);

    bool data_found = false;

    // this function is documented incorrectly. i believe the passed
    // function returns the element type and stops when returning anything
    // that isn't null. the greater EachElement function returns whatever
    // the last returned pointer is and NULL if it goes through each
    // element. the documentation says the passed function returns a
    // boolean.
    // TODO confirm this interpretation is correct
    // TODO make sure this works
    data_found =
        storage->EachElement(
            [](std::tuple<BString, uint32, BString> *i,
               void *params) -> std::tuple<BString, uint32, BString> * {
                BString &attribute_data = *static_cast<BString *>(params);
                if (std::get<0>(*i) == attribute_data) {
                    std::get<1>(*i)++;
                    return i;
                }
                return nullptr;
            },
            &attribute_data) != nullptr;
    if (!data_found) {
        BPath entry_path;
        entry.GetPath(&entry_path);
        storage->AddItem(new std::tuple<BString, uint32, BString>(
            attribute_data, 1, entry_path.Path()));
        BStringList split_list(3); // TODO check for performance
        attribute.Split(":", false, split_list);
        BString attribute_name;
        if (split_list.CountStrings() == 2) {
            attribute_name = split_list.StringAt(1).ToLower();
        } else {
            attribute_name = attribute.ToLower(); // TODO ensure ToLower is good
        }
        return BString("Found ")
            .Append(attribute_name)
            .Append(" \"")
            .Append(attribute_data)
            .Append("\"\n");
    }
    return BString();
}

BString storeTrack(BEntry *track, BDirectory *destination) {
    BPath track_path(track);

    BEntry old_link_entry;
    destination->FindEntry(BString("tracks/").Append(track_path.Leaf()),
                           &old_link_entry, false);
    // TODO determine if necessary to do InitCheck
    if (old_link_entry.InitCheck() == B_OK) {
        old_link_entry.Remove(); // DANGEROUS; DELETES FILE
    }

    BSymLink track_link;
    destination->CreateSymLink(BString("tracks/").Append(track_path.Leaf()),
                               track_path.Path(), &track_link);

    BNode track_node(track);
    BObjectList<BString, true> include(2);
    include.AddItem(new BString(".*"));
    BObjectList<BString, true> exclude(2);
    exclude.AddItem(new BString("BEOS:.*"));
    copyAttributes(&track_node, &track_link, include, exclude);

    return BString("Found track \"").Append(track_path.Leaf()).Append("\"\n");
}

void generateAlbumsAndSingles(
    const BString &destination_path,
    const BObjectList<std::tuple<BString, uint32, BString>, true>
        &album_storage) {
    const BString albums_path(BString(destination_path).Append("/albums/"));
    const BString singles_path(BString(destination_path).Append("/singles/"));

    std::tuple<const BString &, const BString &> params(albums_path,
                                                        singles_path);
    album_storage.EachElement(
        [](const std::tuple<BString, uint32, BString> *element,
           void *params) -> const std::tuple<BString, uint32, BString> * {
            const std::tuple<const BString &, const BString &> &vars =
                *static_cast<std::tuple<const BString &, const BString &> *>(
                    params);
            const BString &albums_path = std::get<0>(vars);
            const BString &singles_path = std::get<1>(vars);
            const std::tuple<BString, uint32, BString> &i = *element;

            if (std::get<1>(i) == 1) {
                std::cout << "Found single \"" << std::get<0>(i) << "\"\n";
            } else {
                std::cout << "Found album \"" << std::get<0>(i) << "\" with "
                          << std::get<1>(i) << " tracks\n";
            }
            if (std::get<1>(i) == 1) {
                std::filesystem::path original_path(std::get<2>(i).String());
                if (std::filesystem::is_symlink(original_path)) {
                    original_path =
                        std::filesystem::read_symlink(original_path);
                }
                std::filesystem::path link_path(
                    BString(singles_path)
                        .Append(original_path.filename().c_str())
                        .String());
                if (std::filesystem::exists(link_path)) {
                    std::filesystem::remove(
                        link_path); // DANGEROUS; DELETES FILE
                }
                std::filesystem::create_symlink(original_path, link_path);
                BNode original_node(original_path.c_str());
                BNode link_node(link_path.c_str());

                BObjectList<BString, true> include(2);
                include.AddItem(new BString(".*"));
                BObjectList<BString, true> exclude(2);
                exclude.AddItem(new BString("BEOS:.*"));
                copyAttributes(&original_node, &link_node, include, exclude);
                return nullptr;
            }

            std::filesystem::path query_path(
                BString(albums_path).Append(std::get<0>(i)).String());
            if (std::filesystem::exists(query_path)) {
                std::filesystem::remove(query_path); // DANGEROUS; DELETES FILE
            }
            std::ofstream query_file(query_path);
            query_file.close(); // this creates an empty file
            BNode query_node(query_path.c_str());

            BString query_mime_type("application/x-vnd.Be-query");
            query_node.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0,
                                 query_mime_type, query_mime_type.Length());
            BString query_string(
                BString("BEOS:TYPE == audio/* && Audio:Album == \"")
                    .Append(std::get<0>(i))
                    .Append("\""));
            query_node.WriteAttr("_trk/qrystr", B_STRING_TYPE, 0, query_string,
                                 query_string.Length());

            // TODO possibly get album_artist from vorbis tags

            BObjectList<BString, true> include(2);
            include.AddItem(new BString("Audio:Artist"));
            include.AddItem(new BString("Media:Genre"));
            include.AddItem(new BString("Media:Year"));
            BObjectList<BString, true> exclude(2);
            exclude.AddItem(new BString(""));
            // get artist, genre, and year from first track in album
            BNode first_track_node(std::get<2>(i));
            copyAttributes(&first_track_node, &query_node, include, exclude);
            return nullptr;
        },
        &params);
}

void generateArtists(
    const BString &destination_path,
    const BObjectList<std::tuple<BString, uint32, BString>, true> &storage) {
    const BString artists_subpath("./artists/");
    BDirectory destination_directory(destination_path);

    std::tuple<const BString &, BDirectory &> params(artists_subpath,
                                                     destination_directory);
    storage.EachElement(
        [](const std::tuple<BString, uint32, BString> *element,
           void *params) -> const std::tuple<BString, uint32, BString> * {
            const std::tuple<const BString &, BDirectory &> &vars =
                *static_cast<std::tuple<const BString &, BDirectory &> *>(
                    params);
            const BString &artists_subpath = std::get<0>(vars);
            BDirectory &destination_directory = std::get<1>(vars);
            const std::tuple<BString, uint32, BString> &i = *element;

            std::cout << "Found artist \"" << std::get<0>(i) << "\"\n";

            BFile artist_file;
            if (destination_directory.CreateFile(
                    BString(artists_subpath).Append(std::get<0>(i)),
                    &artist_file, false) != B_OK) {
                return nullptr;
            }
            BString query_mime_type("application/x-vnd.Be-query");
            artist_file.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0,
                                  query_mime_type, query_mime_type.Length());
            BString query_string(
                BString("BEOS:TYPE == audio/* && Audio:Artist == \"")
                    .Append(std::get<0>(i))
                    .Append("\""));
            artist_file.WriteAttr("_trk/qrystr", B_STRING_TYPE, 0, query_string,
                                  query_string.Length());
            return nullptr;
        },
        &params);
}

export enum ProcessTracksFlags {
    ALBUMS = 0b00000001,
    ARTISTS = 0b00000010,
    GENRES = 0b00000100,
    TRACKS = 0b00001000,
};

export struct ProcessTracksData {
    BQuery *music_query;
    BString source_path;
    BString destination_path;
    uint8 flags;
    BLooper *caller;
};

export status_t processTracks(void *data) {
    ProcessTracksData *args = static_cast<ProcessTracksData *>(data);

    BDirectory destination(args->destination_path);
    // TODO ensure this checks if destination exists correctly
    if (destination.InitCheck() != B_OK) {
        return B_ERROR;
    }

    if (args->flags & ALBUMS) {
        const BString albums_subpath("albums");
        if (!BEntry(&destination, albums_subpath).Exists()) {
            destination.CreateDirectory(albums_subpath, nullptr);
        }
        const BString singles_subpath("singles");
        if (!BEntry(&destination, singles_subpath).Exists()) {
            destination.CreateDirectory(singles_subpath, nullptr);
        }
    }
    if (args->flags & ARTISTS) {
        const BString artists_subpath("artists");
        if (!BEntry(&destination, artists_subpath).Exists()) {
            destination.CreateDirectory(artists_subpath, nullptr);
        }
    }
    if (args->flags & GENRES) {
        const BString genres_subpath("genres");
        if (!BEntry(&destination, genres_subpath).Exists()) {
            destination.CreateDirectory(genres_subpath, nullptr);
        }
    }
    if (args->flags & TRACKS) {
        const BString tracks_subpath("tracks");
        if (!BEntry(&destination, tracks_subpath).Exists()) {
            destination.CreateDirectory(tracks_subpath, nullptr);
        }
    }

    BMessage beginning_line_message(LINE_FROM_PROCESS);
    beginning_line_message.AddString("line",
                                     BString("Beginning processing!\n\n"));
    args->caller->PostMessage(&beginning_line_message);

    BObjectList<std::tuple<BString, uint32, BString>, true> album_storage;
    BObjectList<std::tuple<BString, uint32, BString>, true> artist_storage;
    BEntry entry;
    while (args->music_query->GetNextEntry(&entry) == B_OK) {
        BPath entry_path;
        entry.GetPath(&entry_path);
        BString track_path(entry_path.Path());
        if (!track_path.StartsWith(args->source_path)) {
            continue;
        }

        std::cout << track_path << std::endl;

        BString new_line;
        if (args->flags & ALBUMS) {
            new_line.Prepend(
                storeFromAttribute("Audio:Album", entry, &album_storage));
        }
        if (args->flags & ARTISTS) {
            new_line.Prepend(
                storeFromAttribute("Audio:Artist", entry, &artist_storage));
        }
        if (args->flags & GENRES) {
            // TODO storeGenre
        }
        if (args->flags & TRACKS) {
            new_line.Prepend(storeTrack(&entry, &destination));
        }
        if (!new_line.IsEmpty()) {
            BMessage new_line_message(LINE_FROM_PROCESS);
            new_line_message.AddString("line", new_line);
            args->caller->PostMessage(&new_line_message);
        }
    }
    if (args->flags & ALBUMS) {
        // TODO use BPath or BDirectory instead of BString
        generateAlbumsAndSingles(args->destination_path, album_storage);
    }
    if (args->flags & ARTISTS) {
        generateArtists(args->destination_path, artist_storage);
    }
    if (args->flags & GENRES) {
        // TODO generateGenres
    }

    BMessage finished_line_message(LINE_FROM_PROCESS);
    finished_line_message.AddString("line",
                                    BString("Finished processing!\n\n"));
    args->caller->PostMessage(&finished_line_message);

    BMessage finished_message(FINISHED_PROCESS);
    args->caller->PostMessage(&finished_message);

    delete args;

    return B_OK;
}
