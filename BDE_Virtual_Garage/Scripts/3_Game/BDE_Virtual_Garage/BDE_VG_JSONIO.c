// BDE Virtual Garage: file IO uses Expansion's scripted JSON tree, NOT native
// JsonSerializer.ReadFromString. This keeps disk JSON entirely separate from
// singleton/runtime objects and avoids the observed ReadFromString AV on 1.30.
class BDE_VG_JSONIO
{
    // Match vanilla JsonFileLoader capacity so large stored-vehicle indexes
    // are not silently truncated during verification or Hive refresh.
    static const int MAX_JSON_BYTES = 100000000;
    // A Hive transfer can expose a zero-byte/partial owner index. An empty
    // string passed into ExpansionJSON.FromString raises an engine VM exception
    // even when the caller checks the returned parsed flag. Screen the file
    // BEFORE invoking any JSON parser, and deserialize from this exact snapshot.
    // Read every line using the same FileMode.READ / FGets pattern used by
    // ExpansionJsonFileParser.Load. The former ReadFile() verification path was
    // not reliable for multi-kilobyte garage indexes. Never interpret a failed
    // read as an empty owner index.
    static bool ReadCompleteText(string path, out string contents)
    {
        contents = string.Empty;
        if (!FileExist(path))
            return false;

        FileHandle file = OpenFile(path, FileMode.READ);
        if (!file)
        {
            Print("[BDE Virtual Garage] Cannot open JSON for reading: " + path);
            return false;
        }

        string line;
        int totalLength = 0;
        int lineCount = 0;
        while (FGets(file, line) >= 0)
        {
            int lineLength = line.Length();
            if (lineLength > MAX_JSON_BYTES - totalLength)
            {
                CloseFile(file);
                contents = string.Empty;
                Print("[BDE Virtual Garage] JSON exceeds supported read size: " + path);
                return false;
            }

            // Keep the original line boundaries, as ExpansionJSON.Load does.
            if (lineCount++)
                contents += "\n";
            contents += line;
            totalLength += lineLength + 1;
        }
        CloseFile(file);
        return true;
    }

    // FGets strips line terminators, while our pretty JSON serializer writes
    // them. Compare identical logical JSON text without CR/LF formatting.
    // Literal newline characters are not valid inside JSON strings (escaped
    // \n sequences stay intact), so this does not change string values.
    static bool SameText(string leftText, string rightText)
    {
        int leftIndex;
        int rightIndex;
        while (leftIndex < leftText.Length() || rightIndex < rightText.Length())
        {
            string leftCharacter = string.Empty;
            string rightCharacter = string.Empty;
            while (leftIndex < leftText.Length())
            {
                leftCharacter = leftText.Substring(leftIndex, 1);
                leftIndex++;
                if (leftCharacter != "\r" && leftCharacter != "\n")
                    break;
                leftCharacter = string.Empty;
            }
            while (rightIndex < rightText.Length())
            {
                rightCharacter = rightText.Substring(rightIndex, 1);
                rightIndex++;
                if (rightCharacter != "\r" && rightCharacter != "\n")
                    break;
                rightCharacter = string.Empty;
            }
            if (leftCharacter != rightCharacter)
                return false;
        }
        return true;
    }

    static ExpansionJSON ReadObjectWithText(string path, out string contents)
    {
        if (!ReadCompleteText(path, contents))
            return null;

        ExpansionJSON document = ParseObject(contents);
        if (!document)
            Print("[BDE Virtual Garage] JSON object parse failed; file untouched: " + path + " (read chars=" + contents.Length().ToString() + ")");
        return document;
    }

    // The old Trim/Substring bracket scanner rejected complete, valid JSON
    // above roughly 8 KB before the actual parser ran. Do not rewrite, trim or
    // manually tokenize the snapshot. Expansion's parser validates the whole
    // document, including quoted/escaped braces and incomplete Hive transfers.
    static ExpansionJSON ParseObject(string contents)
    {
        // Empty input previously raised an engine VM exception. An object must
        // contain an opening brace; this cheap guard also excludes whitespace
        // without normalizing/copying a potentially large JSON string.
        if (contents.Length() < 2 || contents.IndexOf("{") < 0)
            return null;
        bool parsed;
        ExpansionJSON document = ExpansionJSON.FromString(contents, parsed);
        if (!parsed || !document || !document.IsObject())
            return null;
        return document;
    }

    static ExpansionJSON ReadObject(string path)
    {
        string contents;
        return ReadObjectWithText(path, contents);
    }

    // A settings/index write first targets a staging file, then its verified
    // contents are explicitly written to the live destination and read back.
    // This is not atomic with a Hive sync or a process crash. Keep the previous
    // snapshot and staging file when publication fails.
    static bool WriteObject(string path, ExpansionJSONObject data)
    {
        if (!data)
            return false;
        return WriteText(path, data.ToJSONString(true));
    }

    static bool WriteText(string path, string contents)
    {
        ExpansionJSON validation = ParseObject(contents);
        if (!validation)
        {
            Print("[BDE Virtual Garage] Refusing to write invalid JSON object: " + path + " chars=" + contents.Length().ToString());
            return false;
        }

        // A .writing file is not a published index. Never replace the live
        // index unless all staged contents have been read back successfully.
        string staging = path + ".writing";
        if (FileExist(staging) && !DeleteFile(staging))
        {
            Print("[BDE Virtual Garage] Cannot clear stale JSON staging file: " + staging);
            return false;
        }
        FileHandle stagingFile = OpenFile(staging, FileMode.WRITE);
        if (!stagingFile)
        {
            Print("[BDE Virtual Garage] Cannot open JSON staging file for writing: " + staging);
            return false;
        }
        FPrint(stagingFile, contents);
        CloseFile(stagingFile);

        string expected = contents;
        string staged;
        if (!ReadCompleteText(staging, staged) || !SameText(staged, expected))
        {
            Print("[BDE Virtual Garage] Staged JSON verification failed: " + path);
            DeleteFile(staging);
            return false;
        }

        bool hadOriginal = FileExist(path);
        string backup = path + ".bak";
        if (hadOriginal)
        {
            // If a Hive transfer left this index unreadable, fail closed
            // without overwriting it OR replacing a valid old backup.
            if (!ReadObject(path))
            {
                Print("[BDE Virtual Garage] Refusing to overwrite unreadable JSON: " + path);
                DeleteFile(staging);
                return false;
            }
            string oldContents;
            if (!ReadCompleteText(path, oldContents))
            {
                DeleteFile(staging);
                return false;
            }
            if (FileExist(backup) && !DeleteFile(backup))
            {
                DeleteFile(staging);
                return false;
            }
            if (!CopyFile(path, backup) || !ReadCompleteText(backup, staged) || !SameText(staged, oldContents))
            {
                Print("[BDE Virtual Garage] JSON backup failed, write aborted: " + path);
                DeleteFile(staging);
                return false;
            }
        }

        // WRITE explicitly replaces an existing file; CopyFile overwrite semantics
        // are not a transaction API. The staged snapshot and backup remain on failure.
        if (WriteVerifiedText(path, contents))
        {
            DeleteFile(staging);
            return true;
        }
        Print("[BDE Virtual Garage] JSON commit verification failed; retaining staging and backup: " + path);
        return false;
    }

    static bool WriteVerifiedText(string path, string contents)
    {
        FileHandle file = OpenFile(path, FileMode.WRITE);
        if (!file)
            return false;
        FPrint(file, contents);
        CloseFile(file);
        string actual;
        return ReadCompleteText(path, actual) && SameText(actual, contents);
    }

    static int ReadInt(ExpansionJSONElementBase objectData, string name, int fallback)
    {
        if (!objectData)
            return fallback;
        ExpansionJSONElementBase value = objectData.Get(name);
        if (!value || value.IsUndefined() || value.IsNull())
            return fallback;
        return value.GetInt();
    }

    static bool ReadBool(ExpansionJSONElementBase objectData, string name, bool fallback)
    {
        if (!objectData)
            return fallback;
        ExpansionJSONElementBase value = objectData.Get(name);
        if (!value || value.IsUndefined() || value.IsNull())
            return fallback;
        return value.GetBool();  // Supports true/false and the existing 1/0 files.
    }

    static float ReadFloat(ExpansionJSONElementBase objectData, string name, float fallback)
    {
        if (!objectData)
            return fallback;
        ExpansionJSONElementBase value = objectData.Get(name);
        if (!value || value.IsUndefined() || value.IsNull())
            return fallback;
        return value.GetFloat();
    }

    static string ReadString(ExpansionJSONElementBase objectData, string name, string fallback)
    {
        if (!objectData)
            return fallback;
        ExpansionJSONElementBase value = objectData.Get(name);
        if (!value || value.IsUndefined() || value.IsNull())
            return fallback;
        return value.GetString();
    }

    static ExpansionJSONObject MakeObject()
    {
        return new ExpansionJSONObject;
    }

    static void PutInt(ExpansionJSONObject objectData, string key, int value)
    {
        objectData.Set(key, new ExpansionJSONInt(value));
    }

    static void PutBool(ExpansionJSONObject objectData, string key, bool value)
    {
        objectData.Set(key, new ExpansionJSONBool(value));
    }

    static void PutFloat(ExpansionJSONObject objectData, string key, float value)
    {
        objectData.Set(key, new ExpansionJSONFloat(value));
    }

    static void PutString(ExpansionJSONObject objectData, string key, string value)
    {
        objectData.Set(key, new ExpansionJSONString(value));
    }
};
