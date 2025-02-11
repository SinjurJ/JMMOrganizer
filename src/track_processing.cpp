module;

// TODO avoid excessive use of standard library
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

#include <Directory.h>
#include <Entry.h>
#include <Looper.h>
#include <Node.h>
#include <Path.h>
#include <Query.h>
#include <String.h>
#include <StringList.h>
#include <SupportDefs.h>
#include <SymLink.h>
#include <TypeConstants.h>
#include <fs_attr.h>

export module track_processing;

import utilities;

void copyAttributes(BNode source, BNode destination,
                    std::vector<std::string> include,
                    std::vector<std::string> exclude) {
    std::array<char, B_ATTR_NAME_LENGTH> attr_name = {0};
    attr_info info = {0, 0};
    std::array<std::byte, B_ATTR_NAME_LENGTH> buffer;

    while (source.GetNextAttrName(attr_name.data()) == B_OK) {
        for (auto i : include) {
            if (std::regex_match(attr_name.begin(), attr_name.end(),
                                 std::regex(i + "\\0.*"))) {
                goto successful_include;
            }
        }
        goto end_of_loop; // TODO this is gross and should be removed
    successful_include:;
        for (auto i : exclude) {
            if (std::regex_match(attr_name.begin(), attr_name.end(),
                                 std::regex(i + "\\0.*"))) {
                goto end_of_loop;
            }
        }
        source.GetAttrInfo(attr_name.data(), &info);
        source.ReadAttr(attr_name.data(), info.type, 0, &buffer, info.size);
        destination.WriteAttr(attr_name.data(), info.type, 0, &buffer,
                              info.size);

    end_of_loop:;
    }
}

BString storeFromAttribute(
    BString attribute, BEntry entry,
    std::vector<std::tuple<std::string, int, std::string>> *storage) {
    BNode track_node = BNode(&entry);
    BString attribute_data;
    track_node.ReadAttrString(attribute, &attribute_data);

    bool data_found = false;
    for (auto &i : *storage) {
        if (std::get<0>(i) == attribute_data.String()) {
            std::get<1>(i)++;
            data_found = true;
            break;
        }
    }
    if (!data_found) {
        BPath entry_path;
        entry.GetPath(&entry_path);
        storage->push_back(
            std::make_tuple(attribute_data.String(), 1, entry_path.Path()));
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

void generateAlbumsAndSingles(
    const std::string &destination_path,
    const std::vector<std::tuple<std::string, int, std::string>>
        &album_storage) {
    const std::string albums_path = destination_path + "/albums/";
    const std::string singles_path = destination_path + "/singles/";

    if (!BEntry(albums_path.c_str()).Exists()) {
        BDirectory albums_directory;
        albums_directory.CreateDirectory(albums_path.c_str(), nullptr);
    }
    if (!BEntry(singles_path.c_str()).Exists()) {
        BDirectory singles_directory;
        singles_directory.CreateDirectory(singles_path.c_str(), nullptr);
    }

    for (auto i : album_storage) {
        if (std::get<1>(i) == 1) {
            std::cout << "Found single \"" << std::get<0>(i) << '\n';
        } else {
            std::cout << "Found album \"" << std::get<0>(i) << "\" with "
                      << std::get<1>(i) << " tracks\n";
        }
        if (std::get<1>(i) == 1) {
            std::filesystem::path original_path(std::get<2>(i));
            if (std::filesystem::is_symlink(original_path)) {
                original_path = std::filesystem::read_symlink(original_path);
            }
            std::filesystem::path link_path(singles_path +
                                            original_path.filename().string());
            if (std::filesystem::exists(link_path)) {
                std::filesystem::remove(link_path); // DANGEROUS; DELETES FILE
            }
            std::filesystem::create_symlink(original_path, link_path);
            BNode original_node(original_path.c_str());
            BNode link_node(link_path.c_str());

            copyAttributes(original_node, link_node, {".*"}, {"BEOS:.*"});
            continue;
        }

        std::filesystem::path query_path(albums_path + std::get<0>(i));
        if (std::filesystem::exists(query_path)) {
            std::filesystem::remove(query_path); // DANGEROUS; DELETES FILE
        }
        std::ofstream query_file(query_path);
        query_file.close(); // this creates an empty file
        BNode query_node(query_path.c_str());

        std::string query_mime_type = "application/x-vnd.Be-query";
        query_node.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0,
                             query_mime_type.c_str(), query_mime_type.length());
        std::string query_string("BEOS:TYPE == audio/* && Audio:Album == \"" +
                                 std::get<0>(i) + "\"");
        query_node.WriteAttr("_trk/qrystr", B_STRING_TYPE, 0,
                             query_string.c_str(), query_string.length());

        // TODO possibly get album_artist from vorbis tags
        
        // get artist, genre, and year from first track in album
        BNode first_track_node(std::get<2>(i).c_str());
        copyAttributes(first_track_node, query_node,
                       {"Audio:Artist", "Media:Genre", "Media:Year"}, {""});
    }
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

export int processTracks(void *data) {
    ProcessTracksData *args = static_cast<ProcessTracksData *>(data);

    BMessage beginning_line_message(LINE_FROM_PROCESS);
    beginning_line_message.AddString("line",
                                     BString("Beginning processing!\n\n"));
    args->caller->PostMessage(&beginning_line_message);

    std::vector<std::tuple<std::string, int, std::string>> album_storage;
    std::vector<std::tuple<std::string, int, std::string>> artist_storage;
    BEntry entry;
    while (args->music_query->GetNextEntry(&entry) == B_OK) {
        BPath entry_path;
        entry.GetPath(&entry_path);
        std::string track_path(entry_path.Path());
        if (track_path.find(args->source_path) != 0) {
            continue;
        }

        std::cout << track_path << std::endl;

        BString new_line;
        if (args->flags & ALBUMS) {
            new_line.Append(
                storeFromAttribute("Audio:Album", entry, &album_storage));
        }
        if (args->flags & ARTISTS) {
            new_line.Append(
                storeFromAttribute("Audio:Artist", entry, &artist_storage));
        }
        if (args->flags & GENRES) {
            // TODO storeGenre
        }
        if (args->flags & TRACKS) {
            // TODO storeTrack
        }
        if (!new_line.IsEmpty()) {
            BMessage new_line_message(LINE_FROM_PROCESS);
            new_line_message.AddString("line", new_line);
            args->caller->PostMessage(&new_line_message);
        }
    }
    if (args->flags & ALBUMS) {
        generateAlbumsAndSingles(std::string(args->destination_path),
                                 album_storage);
    }
    if (args->flags & ARTISTS) {
        // TODO generateArtists
    }
    if (args->flags & GENRES) {
        // TODO generateGenres
    }
    if (args->flags & TRACKS) {
        // TODO generateTracks
    }

    BMessage finished_line_message(LINE_FROM_PROCESS);
    finished_line_message.AddString("line",
                                    BString("Finished processing!\n\n"));
    args->caller->PostMessage(&finished_line_message);

    BMessage finished_message(FINISHED_PROCESS);
    args->caller->PostMessage(&finished_message);

    delete args;

    return 0;
}
