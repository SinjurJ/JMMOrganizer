export module utilities;

export enum process_message_code {
    ACTIVATE_ALBUMS = 'albu',
    ACTIVATE_ARTISTS = 'arti',
    ACTIVATE_GENRES = 'genr',
    ACTIVATE_SINGLES = 'sing',
    ACTIVATE_TRACKS = 'trac',
    FINISHED_PROCESS = 'fini',
    GENERATE = 'gene',
    LINE_FROM_PROCESS = 'line',
    SETTINGS = 'sett',
    SETTINGS_APPLY = 'stap',
    SETTINGS_BROWSE_DESTINATION = 'stbd',
    SETTINGS_BROWSE_SOURCE = 'stbs',
    SETTINGS_CLOSED = 'stcl',
    SETTINGS_REFS_DESTINATION = 'strd',
    SETTINGS_REFS_SOURCE = 'strs',
    SETTINGS_REQUESTED = 'strq',
    SETTINGS_REVERT = 'strv',
    SETTINGS_MODIFIED = 'stmd',
};
