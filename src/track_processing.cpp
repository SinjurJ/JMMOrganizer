module;

// TODO avoid excessive use of standard library
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
    if (data_found) {
        return BString();
    }
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
    if (attribute_data.IsEmpty()) {
        return BString("Found empty ").Append(attribute_name).Append("\n");
    }
    return BString("Found ")
        .Append(attribute_name)
        .Append(" \"")
        .Append(attribute_data)
        .Append("\"\n");
}

void removeEntry(BString entry_path, BDirectory *destination) {
    BEntry entry;
    // TODO determine if necessary to do InitCheck
    destination->FindEntry(entry_path, &entry, false);
    if (entry.InitCheck() == B_OK) {
        entry.Remove(); // DANGEROUS; DELETES FILE
    }
}

BString cleanName(const BString &name) {
    return BString(name).ReplaceAll('/', '-');
}

status_t generateQuery(const BString &name, const BString &type,
                       BFile *query_file, BDirectory *destination,
                       const BString &subpath) {
    if (name.IsEmpty()) {
        return B_ERROR;
    }

    const BString query_path(BString(subpath).Append(cleanName(name)));

    removeEntry(query_path, destination);

    if (destination->CreateFile(query_path, query_file, false) != B_OK) {
        return B_ERROR;
    }

    const BString query_mime_type("application/x-vnd.Be-query");
    query_file->WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0, query_mime_type,
                          query_mime_type.Length());
    const BString query_string(BString("BEOS:TYPE == audio/* && ")
                                   .Append(type)
                                   .Append(" == \"")
                                   .Append(name)
                                   .Append("\""));
    query_file->WriteAttr("_trk/qrystr", B_STRING_TYPE, 0, query_string,
                          query_string.Length());

    return B_OK;
}

status_t generateAlbum(const BString &album, const BEntry &first_track,
                       BDirectory *destination, const BString &subpath) {
    BFile query_file;
    if (generateQuery(album, "Audio:Album", &query_file, destination,
                      subpath) != B_OK) {
        return B_ERROR;
    }

    // TODO possibly get album_artist from vorbis tags
    BObjectList<BString, true> include(4);
    include.AddItem(new BString("Audio:Artist"));
    include.AddItem(new BString("Media:Genre"));
    include.AddItem(new BString("Media:Year"));
    BObjectList<BString, true> exclude(2);
    exclude.AddItem(new BString("BEOS:.*"));
    BNode first_track_node(&first_track);
    copyAttributes(&first_track_node, &query_file, include, exclude);

    return B_OK;
}

status_t generateArtist(const BString &artist, BDirectory *destination,
                        const BString &subpath) {
    BFile query_file;
    return generateQuery(artist, "Audio:Artist", &query_file, destination,
                         subpath);
}

status_t generateGenre(const BString &genre, BDirectory *destination,
                       const BString &subpath) {
    BFile query_file;
    return generateQuery(genre, "Media:Genre", &query_file, destination,
                         subpath);
}

status_t generateTrack(const BEntry &track, BDirectory *destination,
                       const BString &subpath) {
    const BPath track_path(&track);
    const BString link_path(
        BString(subpath).Append(cleanName(track_path.Leaf())));

    removeEntry(link_path, destination);

    BSymLink track_link;
    if (destination->CreateSymLink(link_path, track_path.Path(), &track_link) !=
        B_OK) {
        return B_ERROR;
    }

    BNode track_node(&track);
    BObjectList<BString, true> include(2);
    include.AddItem(new BString(".*"));
    BObjectList<BString, true> exclude(2);
    exclude.AddItem(new BString("BEOS:.*"));
    copyAttributes(&track_node, &track_link, include, exclude);

    return B_OK;
}

export enum ProcessTracksFlags {
    ALBUMS = 0b00000001,
    ARTISTS = 0b00000010,
    GENRES = 0b00000100,
    SINGLES = 0b00001000,
    TRACKS = 0b00010000,
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
        BMessage destination_not_found_message(LINE_FROM_PROCESS);
        destination_not_found_message.AddString(
            "line", BString("Failed to access the destination folder\n\n"));
        args->caller->PostMessage(&destination_not_found_message);

        // TODO don't repeat finished_message code
        BMessage finished_message(FINISHED_PROCESS);
        args->caller->PostMessage(&finished_message);
        return B_ERROR;
    }

    // TODO print errors to cerr when things fail instead of failing silently

    const BString albums_subpath("albums/");
    const BString artists_subpath("artists/");
    const BString genres_subpath("genres/");
    const BString singles_subpath("singles/");
    const BString tracks_subpath("tracks/");
    if (args->flags & ALBUMS) {
        if (!BEntry(&destination, albums_subpath).Exists()) {
            destination.CreateDirectory(albums_subpath, nullptr);
        }
    }
    if (args->flags & ARTISTS) {
        if (!BEntry(&destination, artists_subpath).Exists()) {
            destination.CreateDirectory(artists_subpath, nullptr);
        }
    }
    if (args->flags & GENRES) {
        if (!BEntry(&destination, genres_subpath).Exists()) {
            destination.CreateDirectory(genres_subpath, nullptr);
        }
    }
    if (args->flags & SINGLES) {
        if (!BEntry(&destination, singles_subpath).Exists()) {
            destination.CreateDirectory(singles_subpath, nullptr);
        }
    }
    if (args->flags & TRACKS) {
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
    BObjectList<std::tuple<BString, uint32, BString>, true> genre_storage;
    BEntry entry;
    while (args->music_query->GetNextEntry(&entry) == B_OK) {
        BPath entry_path(&entry);
        BString track_path(entry_path.Path());
        if (!track_path.StartsWith(args->source_path)) {
            continue;
        }

        std::cout << track_path << "\n";

        BString new_line;
        if (args->flags & ALBUMS || args->flags & SINGLES) {
            new_line.Prepend(
                storeFromAttribute("Audio:Album", entry, &album_storage));
        }
        if (args->flags & ARTISTS) {
            new_line.Prepend(
                storeFromAttribute("Audio:Artist", entry, &artist_storage));
        }
        if (args->flags & GENRES) {
            new_line.Prepend(
                storeFromAttribute("Media:Genre", entry, &genre_storage));
        }
        if (args->flags & TRACKS) {
            // TODO wait to generate tracks until after loop
            new_line.Prepend(BString("Found track \"")
                                 .Append(entry_path.Leaf())
                                 .Append("\"\n"));
            if (generateTrack(entry, &destination, tracks_subpath) != B_OK) {
                std::cerr << "Failed to generate track \"" << entry_path.Leaf()
                          << "\"\n";
            }
        }
        if (!new_line.IsEmpty()) {
            BMessage new_line_message(LINE_FROM_PROCESS);
            new_line_message.AddString("line", new_line);
            args->caller->PostMessage(&new_line_message);
        }

        std::cerr.flush();
        std::cout.flush();
    }

    if (args->flags & ALBUMS || args->flags & SINGLES) {
        for (auto i = 0; i < album_storage.CountItems(); i++) {
            const std::tuple<BString, uint32, BString> &item =
                *album_storage.ItemAt(i);
            const BEntry entry(std::get<2>(item), false);
            const BPath path(&entry);
            if (args->flags & SINGLES) {
                if (std::get<1>(item) == 1) {
                    if (generateTrack(entry, &destination, singles_subpath) !=
                        B_OK) {
                        std::cerr << "Failed to generate single \""
                                  << path.Leaf() << "\"\n";
                    }
                }
            }
            if (args->flags & ALBUMS) {
                if (std::get<1>(item) > 1) {
                    if (generateAlbum(std::get<0>(item), entry, &destination,
                                      albums_subpath) != B_OK) {
                        std::cerr << "Failed to generate album \""
                                  << std::get<0>(item) << "\"\n";
                    }
                }
            }
        }
    }
    if (args->flags & ARTISTS) {
        for (auto i = 0; i < artist_storage.CountItems(); i++) {
            const BString &artist = std::get<0>(*artist_storage.ItemAt(i));
            if (generateArtist(artist, &destination, artists_subpath) != B_OK) {
                std::cerr << "Failed to generate artist \"" << artist << "\"\n";
            }
        }
    }
    if (args->flags & GENRES) {
        for (auto i = 0; i < genre_storage.CountItems(); i++) {
            const BString &genre = std::get<0>(*genre_storage.ItemAt(i));
            if (generateGenre(genre, &destination, genres_subpath) != B_OK) {
                std::cerr << "Failed to generate genre \"" << genre << "\"\n";
            }
        }
    }

    BMessage finished_line_message(LINE_FROM_PROCESS);
    finished_line_message.AddString("line",
                                    BString("Finished processing!\n\n"));
    args->caller->PostMessage(&finished_line_message);

    BMessage finished_message(FINISHED_PROCESS);
    args->caller->PostMessage(&finished_message);

    std::cerr.flush();
    std::cout.flush();

    delete args;

    return B_OK;
}
