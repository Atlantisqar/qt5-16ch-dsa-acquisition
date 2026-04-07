%% read_seismic_data_and_plot.m
% Read Seismic_data_*.bin files produced by the legacy seismic acquisition software.
%
% Binary layout (little-endian), inferred from Joint_Acquisittion_System/mainwindow.cpp:
%   uint32 magic          = hex2dec('44534131')   % "DSA1"
%   uint32 pointsPerGroup = N
%   uint32 groupCount
%   uint32 startGroup
%   uint32 channelCount   = 16
%   [groupCount groups of samples]
%       for each group:
%           for each channel 1..channelCount:
%               N x double
%
% Features:
% - select one or more Seismic_data_*.bin files, or an entire folder
% - choose channels to read
% - choose tail points to keep per selected channel
% - choose tail points to plot

clear; clc;

[filePaths, canceled] = pickSeismicFiles();
if canceled
    fprintf('Canceled.\n');
    return;
end

[selectedChannels, maxReadPointsPerChannel, maxPlotPointsPerChannel, canceled] = pickReadOptions();
if canceled
    fprintf('Canceled.\n');
    return;
end

[data, meta] = readSeismicFiles(filePaths, selectedChannels, maxReadPointsPerChannel);
if meta.loadedPointsPerChannel == 0
    error('No valid data points found in selected files.');
end

selectedChannels = sanitizeChannels(selectedChannels, meta.channelCount);
if isempty(selectedChannels)
    error('No valid channels selected.');
end

plotSeismicChannels(data, meta, selectedChannels, maxPlotPointsPerChannel);

fprintf('Done. Format: %s\n', meta.format);
fprintf('Files used: %d\n', numel(meta.sourceFiles));
fprintf('Points per group: %d\n', meta.pointsPerGroup);
fprintf('Total groups: %d\n', meta.totalGroups);
fprintf('Available points per channel: %d\n', meta.totalAvailablePointsPerChannel);
fprintf('Loaded points per selected channel (tail): %d\n', meta.loadedPointsPerChannel);
fprintf('Selected channels: %s\n', mat2str(selectedChannels));

%% ---------- Local functions ----------
function [filePaths, canceled] = pickSeismicFiles()
    canceled = false;
    filePaths = {};

    choice = questdlg( ...
        'Choose seismic data from individual files or an entire folder?', ...
        'Select Seismic Input', ...
        'Select Files', 'Select Folder', 'Select Files');

    if isempty(choice)
        canceled = true;
        return;
    end

    if strcmp(choice, 'Select Folder')
        folderPath = uigetdir(pwd, 'Select folder containing Seismic_data_*.bin');
        if isequal(folderPath, 0)
            canceled = true;
            return;
        end

        listing = dir(fullfile(folderPath, 'session_*.bin'));
        if isempty(listing)
            listing = dir(fullfile(folderPath, 'seismic*.bin'));
        end
        if isempty(listing)
            error('No Seismic_data_*.bin files found in folder: %s', folderPath);
        end

        names = sort({listing.name});
        filePaths = cellfun(@(n) fullfile(folderPath, n), names, 'UniformOutput', false);
        return;
    end

    [files, pathName] = uigetfile( ...
        {'Seismic_data_*.bin;seismic*.bin', 'Seismic data files (Seismic_data_*.bin)'; ...
         '*.bin', 'Binary files (*.bin)'; ...
         '*.*', 'All files (*.*)'}, ...
        'Select seismic data files', ...
        pwd, ...
        'MultiSelect', 'on');

    if isequal(files, 0)
        canceled = true;
        return;
    end

    if ischar(files)
        files = {files};
    end

    filePaths = cellfun(@(f) fullfile(pathName, f), files, 'UniformOutput', false);
    filePaths = sort(filePaths);
end

function [selectedChannels, maxReadPointsPerChannel, maxPlotPointsPerChannel, canceled] = pickReadOptions()
    canceled = false;
    selectedChannels = 1:16;
    maxReadPointsPerChannel = 0;
    maxPlotPointsPerChannel = 2400;

    channelItems = arrayfun(@(c) sprintf('CH%d', c), 1:16, 'UniformOutput', false);
    [idx, ok] = listdlg('PromptString', 'Select channels to read:', ...
                        'SelectionMode', 'multiple', ...
                        'ListString', channelItems, ...
                        'InitialValue', 1:16, ...
                        'ListSize', [220, 280], ...
                        'Name', 'Read Options');
    if ~ok || isempty(idx)
        canceled = true;
        return;
    end
    selectedChannels = idx(:)';

    answer = inputdlg( ...
        {'Tail points to READ per selected channel (0 = all):', ...
         'Tail points to PLOT per selected channel:'}, ...
        'Read Options', ...
        [1 58; 1 58], ...
        {'0', '2400'});
    if isempty(answer)
        canceled = true;
        return;
    end

    readCount = str2double(strtrim(answer{1}));
    plotCount = str2double(strtrim(answer{2}));
    if isnan(readCount) || readCount < 0 || floor(readCount) ~= readCount
        error('Invalid read point count. Please input a non-negative integer.');
    end
    if isnan(plotCount) || plotCount <= 0 || floor(plotCount) ~= plotCount
        error('Invalid plot point count. Please input a positive integer.');
    end

    maxReadPointsPerChannel = readCount;
    maxPlotPointsPerChannel = plotCount;
end

function [data, meta] = readSeismicFiles(filePaths, selectedChannels, maxReadPointsPerChannel)
    expectedMagic = uint32(hex2dec('44534131'));
    channels = {};
    channelCountExpected = [];
    pointsPerGroupExpected = [];
    totalGroups = 0;
    totalAvailablePointsPerChannel = 0;

    for i = 1:numel(filePaths)
        filePath = filePaths{i};
        fileInfo = dir(filePath);
        fid = fopen(filePath, 'rb', 'ieee-le');
        if fid < 0
            warning('Cannot open file: %s', filePath);
            continue;
        end
        cleaner = onCleanup(@() fclose(fid));

        magic = fread(fid, 1, 'uint32=>uint32');
        pointsPerGroup = fread(fid, 1, 'uint32=>uint32');
        groupCount = fread(fid, 1, 'uint32=>uint32');
        startGroup = fread(fid, 1, 'uint32=>uint32'); %#ok<NASGU>
        channelCount = fread(fid, 1, 'uint32=>uint32');

        if any(cellfun(@isempty, {magic, pointsPerGroup, groupCount, channelCount}))
            warning('Invalid/empty header: %s', filePath);
            clear cleaner;
            continue;
        end

        if magic ~= expectedMagic
            warning('Unexpected magic in file: %s', filePath);
            clear cleaner;
            continue;
        end

        if isempty(channelCountExpected)
            channelCountExpected = double(channelCount);
            pointsPerGroupExpected = double(pointsPerGroup);
            channels = cell(1, channelCountExpected);
            for c = 1:channelCountExpected
                channels{c} = zeros(0, 1);
            end
        end

        if double(channelCount) ~= channelCountExpected
            warning('Skip file with different channel count (%d): %s', channelCount, filePath);
            clear cleaner;
            continue;
        end

        if double(pointsPerGroup) ~= pointsPerGroupExpected
            warning('Skip file with different points/group (%d): %s', pointsPerGroup, filePath);
            clear cleaner;
            continue;
        end

        selectedMask = false(1, channelCountExpected);
        validSelectedChannels = sanitizeChannels(selectedChannels, channelCountExpected);
        selectedMask(validSelectedChannels) = true;

        expectedBytes = 20 + double(pointsPerGroup) * double(groupCount) * double(channelCount) * 8;
        if ~isempty(fileInfo) && double(fileInfo.bytes) < expectedBytes
            warning('File shorter than expected, may be incomplete: %s', filePath);
        end

        stopThisFile = false;
        for g = 1:double(groupCount)
            for ch = 1:channelCountExpected
                if selectedMask(ch)
                    values = fread(fid, [double(pointsPerGroup), 1], 'double=>double');
                    if numel(values) ~= double(pointsPerGroup)
                        warning('Incomplete channel block in file: %s', filePath);
                        stopThisFile = true;
                        break;
                    end
                    channels{ch} = appendAndTrim(channels{ch}, values, maxReadPointsPerChannel);
                else
                    skipBytes = double(pointsPerGroup) * 8;
                    if fseek(fid, skipBytes, 'cof') ~= 0
                        warning('Seek failed in file: %s', filePath);
                        stopThisFile = true;
                        break;
                    end
                end
            end
            if stopThisFile
                break;
            end
        end

        if ~stopThisFile
            totalGroups = totalGroups + double(groupCount);
            totalAvailablePointsPerChannel = totalAvailablePointsPerChannel + double(pointsPerGroup) * double(groupCount);
        end

        clear cleaner;
    end

    loadedPoints = 0;
    if ~isempty(channels)
        loadedPoints = max(cellfun(@numel, channels));
    end

    data = struct('channels', {channels});
    meta = struct('format', 'seismic_bin', ...
                  'channelCount', defaultIfEmpty(channelCountExpected, 16), ...
                  'pointsPerGroup', defaultIfEmpty(pointsPerGroupExpected, 0), ...
                  'totalGroups', totalGroups, ...
                  'totalAvailablePointsPerChannel', totalAvailablePointsPerChannel, ...
                  'loadedPointsPerChannel', loadedPoints, ...
                  'selectedChannels', selectedChannels, ...
                  'maxReadPointsPerChannel', maxReadPointsPerChannel, ...
                  'sourceFiles', {filePaths});
end

function out = appendAndTrim(in, toAppend, maxPoints)
    if isempty(toAppend)
        out = in;
        return;
    end

    out = [in; toAppend]; %#ok<AGROW>
    if maxPoints > 0 && numel(out) > maxPoints
        out = out(end - maxPoints + 1:end);
    end
end

function channels = sanitizeChannels(channels, maxChannel)
    channels = unique(channels(:)');
    channels = channels(channels >= 1 & channels <= maxChannel);
end

function value = defaultIfEmpty(value, fallback)
    if isempty(value)
        value = fallback;
    end
end

function plotSeismicChannels(data, meta, selectedChannels, maxPlotPoints)
    figure('Color', 'w', 'Name', 'Seismic Data Plot', 'NumberTitle', 'off');
    nPlots = numel(selectedChannels);
    topTitle = sprintf('Format: %s | Groups: %d | Points/Ch(loaded): %d', ...
                       meta.format, meta.totalGroups, meta.loadedPointsPerChannel);

    hasTiledLayout = exist('tiledlayout', 'file') == 2;
    hasSgTitle = exist('sgtitle', 'file') == 2;
    hasSupTitle = exist('suptitle', 'file') == 2;

    if hasTiledLayout
        t = tiledlayout(nPlots, 1, 'TileSpacing', 'compact', 'Padding', 'compact');
        title(t, topTitle);
    else
        if hasSgTitle
            sgtitle(topTitle);
        elseif hasSupTitle
            suptitle(topTitle);
        end
    end

    for k = 1:nPlots
        ch = selectedChannels(k);
        y = data.channels{ch};

        if isempty(y)
            yPlot = zeros(0, 1);
        else
            n = numel(y);
            startIdx = max(1, n - maxPlotPoints + 1);
            yPlot = y(startIdx:n);
        end

        if hasTiledLayout
            nexttile;
        else
            subplot(nPlots, 1, k);
        end

        if isempty(yPlot)
            plot(nan, nan);
            text(0.5, 0.5, sprintf('CH%d: no data', ch), 'Units', 'normalized', ...
                'HorizontalAlignment', 'center', 'Color', [0.5 0.5 0.5]);
        else
            plot(yPlot, 'LineWidth', 1.0);
        end
        grid on;
        ylabel(sprintf('CH%d', ch));
        if k == nPlots
            xlabel('Sample Index (tail)');
        end

        if ~hasTiledLayout && ~hasSgTitle && ~hasSupTitle && k == 1
            title(topTitle, 'Interpreter', 'none');
        end
    end
end
