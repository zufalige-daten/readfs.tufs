Tethered Used FileSystem (TUFS)

"TUFS" : % {
    "reserved sectors" : [%, 512b] = {
		"boot sector" : 512b;
		"fs header" : 512b = {
			"signiture (Sig)" : 4b = "TUFS";
			"lowest free sector (LFSect)" : 8b;
			"partition size in sectors (PSISect)" : 8b;
			"partition start sector (PSSect)" : 8b;
			"partition name (PName)" : 11b; # any unused bytes must be space characters (' ')
			"reserved sector count (RSCount)" : 2b; # must be at least 2 (to account for the boot sector and the filesystem header)
            "root directory sector (RDSect)" : 8b;
			"version number (VNumber)" : 2b = 1; # 1 for 1.0
			"reserved (Res)" : %;
		}
		"extended boot loader" : [%, 512b];
	}
    "data blocks" : (((PSILba - LBAOBOPart) - RSCount)*512)b = {
		: "file allocation entry" : 512b = {
			"used (Used)" : 1b; # 0: unused; 1: used (file allocation entry)
			"file attributes (FAttributes)" : 1b;
			: '
			"file attributes [bit, explanation]"{
				0: is file hidden
				1: is file a span file
				2: is file a directory
				3: is file a symbolic link
			}
			'
			"file last modified time (FLMTime)" : 4b; # sint32_t value that represents the number of non-leap seconds since midnight UTC on 1st January 1970 (the UNIX epoch) aka UNIX time (standered)
			"file last read time (FLRTime)" : 4b;
			"file creation time (FCTime)" : 4b;
			"file size in bytes (FSIBytes)" : 8b;
			"file start sector | file symbolic link file entry sector (FSSect)" : 8b; # points to file data sector span descriptor if for a span file or first file sector if is directory
			"user permissions (UPerms)" : 1b;
			:'
				[bit]		[permission]
				0			read
				1			write
				2			create
				3			delete
				4			override
			'
			"owner group / username (OGUsername)" : 16b; // null terminated unless reaches end of 16 bytes
			"file name (FName)" : 457b; # must be null terminated unles file name ends at end of file name area (filename includes any file extenstions: excludes the following characters: { '/' '\' '*' OR ':' })
			"next file in directory sectir (NFIDSect)" : 8b; # if 0: last file in directory
		}
		| "file data sector (dynamic)" : 512b = {
			"used (Used)" : 1b; # 0: unused; 2: used (file data sector)
			"file region file data (FRFData)" : 503b;
			"next file region sector (NFRSect)" : 8b;
		}
		| "file data sector span descriptor" : 512b = {
			"used (Used)" : 1b; # 0: unused; 3: used (file data sector span descriptor)
			"number of sectors reserved for span (NOSRFSpan)" : 8b;
			"reserved (Rsvd)" : 503b;
		} # note: span starts directly after
	}
}
