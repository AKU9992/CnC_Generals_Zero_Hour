void GameStateMap::clearScratchPadMaps()
{
    // AKU9992: keep cleanup inside Save; do not change the process working directory.
    AsciiString directory=TheGameState->getSaveDirectory();
    if(!TheFileSystem->createDirectory(directory)){const DWORD attributes=GetFileAttributes(directory.str());if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))return;}
    AsciiString pattern=directory;pattern.concat("*.map");
    WIN32_FIND_DATA item{};
    HANDLE search=FindFirstFile(pattern.str(),&item);
    if(search==INVALID_HANDLE_VALUE)return;
    do {
        if(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        const char* extension=strrchr(item.cFileName,'.');
        if(!extension || stricmp(extension,".map")!=0)continue;
        AsciiString file=directory;file.concat(item.cFileName);
        DeleteFile(file.str());
    }while(FindNextFile(search,&item));
    FindClose(search);
}
