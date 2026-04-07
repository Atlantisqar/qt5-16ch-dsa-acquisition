%% read_dsa_data_and_plot.m
% Read DSA data files and plot selected channels.
% Supported file selection modes:
% 1) session_*.bin / session_*_partNNN.bin (preferred)
% 2) legacy ch*.bin files
%
% IMPORTANT:
% - This script now supports selecting files directly from a folder.
% - In the file dialog, select one format at a time:
%   either all session files, or all ch*.bin files. Mixed selection is not allowed.
% - After selecting files, you can choose:
%   1) channels to read
%   2) tail points to read per selected channel
%   3) tail points to plot
%   4) sampling rate used for frequency detection
%
% The script estimates the dominant frequency of each plotted channel and
% shows it both in the figure and in the command window.

clear; clc;

[mode, filePaths, canceled] = pickInputFiles();
if canceled
    fprintf('Canceled.\n');
    return;
end

[selectedChannels, maxReadPointsPerChannel, maxPlotPointsPerChannel, sampleRateHz, canceled] = pickReadOptions();
if canceled
    fprintf('Canceled.\n');
    return;
end

[data, meta] = readDsaDataFromFiles(mode, filePaths, selectedChannels, maxReadPointsPerChannel);
if meta.totalPointsPerChannel == 0
    error('No valid data points found in selected files.');
end

selectedChannels = sanitizeChannels(selectedChannels, meta.channelCount);
if isempty(selectedChannels)
    error('No valid channels selected.');
end

freqInfo = detectChannelFrequencies(data, selectedChannels, maxPlotPointsPerChannel, sampleRateHz);

plotDsaChannels(data, meta, selectedChannels, maxPlotPointsPerChannel, freqInfo, sampleRateHz);

fprintf('Done. Source format: %s\n', meta.format);
fprintf('Total points per channel (max): %d\n', meta.totalPointsPerChannel);
fprintf('Selected channels: %s\n', mat2str(selectedChannels));
if maxReadPointsPerChannel > 0
    fprintf('Read tail points per selected channel: %d\n', maxReadPointsPerChannel);
else
    fprintf('Read tail points per selected channel: all\n');
end
fprintf('Sampling rate used for frequency detection: %.3f Hz\n', sampleRateHz);
fprintf('Files used: %d\n', numel(meta.sourceFiles));
printFrequencySummary(freqInfo, selectedChannels);

%% ---------- Local functions ----------
function [mode, filePaths, canceled] = pickInputFiles()
    canceled = false;
    mode = "";
    filePaths = {};

    [files, pathName] = uigetfile( ...
        {'session_*.bin;session_*.dsa16bin', 'DSA session files (session_*.bin, session_*.dsa16bin)'; ...
         'ch*.bin',    'Legacy channel files (ch*.bin)'; ...
         '*.bin;*.dsa16bin', 'Binary files (*.bin, *.dsa16bin)'; ...
         '*.*',        'All files (*.*)'}, ...
        'Select DSA data files (MultiSelect)', ...
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

    names = lower(cellfun(@(p) getName(p), filePaths, 'UniformOutput', false));

    isSession = all(~cellfun('isempty', regexp(names, '^session_.*\.(bin|dsa16bin)$', 'once')));
    isLegacy = all(~cellfun('isempty', regexp(names, '^ch\d+\.bin$', 'once')));

    if isSession
        mode = "session";
        return;
    end
    if isLegacy
        mode = "legacy";
        return;
    end

    error(['Invalid file selection. Please select either:\n' ...
           '1) Only session_*.bin files (or old session_*.dsa16bin)\n' ...
           '2) Only ch*.bin files']);
end

function [selectedChannels, maxReadPointsPerChannel, maxPlotPointsPerChannel, sampleRateHz, canceled] = pickReadOptions()
    canceled = false;
    selectedChannels = 1:16;
    maxReadPointsPerChannel = 2400;
    maxPlotPointsPerChannel = 2400;
    sampleRateHz = 25600;

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
         'Tail points to PLOT per selected channel:', ...
         'Sampling rate in Hz for frequency detection:'}, ...
        'Read Options', ...
        [1 58; 1 58; 1 58], ...
        {'2400', '2400', '25600'});
    if isempty(answer)
        canceled = true;
        return;
    end

    readCount = str2double(strtrim(answer{1}));
    plotCount = str2double(strtrim(answer{2}));
    sampleRate = str2double(strtrim(answer{3}));
    if isnan(readCount) || readCount < 0 || floor(readCount) ~= readCount
        error('Invalid read point count. Please input a non-negative integer.');
    end
    if isnan(plotCount) || plotCount <= 0 || floor(plotCount) ~= plotCount
        error('Invalid plot point count. Please input a positive integer.');
    end
    if isnan(sampleRate) || ~isfinite(sampleRate) || sampleRate <= 0
        error('Invalid sampling rate. Please input a positive number.');
    end
    maxReadPointsPerChannel = readCount;
    maxPlotPointsPerChannel = plotCount;
    sampleRateHz = sampleRate;
end

function [data, meta] = readDsaDataFromFiles(mode, filePaths, selectedChannels, maxReadPointsPerChannel)
    switch mode
        case "session"
            [data, meta] = readSessionFiles(filePaths, selectedChannels, maxReadPointsPerChannel);
        case "legacy"
            [data, meta] = readLegacyChannelFiles(filePaths, selectedChannels, maxReadPointsPerChannel);
        otherwise
            error('Unknown mode: %s', mode);
    end
end

function [data, meta] = readSessionFiles(filePaths, selectedChannels, maxReadPointsPerChannel)
    channelCountExpected = 16;
    channels = cell(1, channelCountExpected);
    for c = 1:channelCountExpected
        channels{c} = zeros(0, 1);
    end
    selectedMask = false(1, channelCountExpected);
    selectedMask(selectedChannels) = true;

    totalFrames = 0;

    for i = 1:numel(filePaths)
        filePath = filePaths{i};
        fid = fopen(filePath, 'rb', 'ieee-le');
        if fid < 0
            warning('Cannot open file: %s', filePath);
            continue;
        end
        cleaner = onCleanup(@() fclose(fid));

        magic = fread(fid, 1, 'uint32=>uint32');
        version = fread(fid, 1, 'uint32=>uint32');
        channelCount = fread(fid, 1, 'uint32=>uint32');

        if isempty(magic) || isempty(version) || isempty(channelCount)
            warning('Invalid/empty header: %s', filePath);
            clear cleaner;
            continue;
        end

        if magic ~= uint32(hex2dec('31415344'))
            warning('Unexpected magic in file: %s', filePath);
            clear cleaner;
            continue;
        end

        if channelCount ~= channelCountExpected
            warning('Unexpected channel count (%d) in file: %s', channelCount, filePath);
            clear cleaner;
            continue;
        end

        stopThisFile = false;
        while ~stopThisFile
            ts = fread(fid, 1, 'uint64=>uint64'); %#ok<NASGU>
            if isempty(ts)
                break;
            end

            pointCount = fread(fid, 1, 'uint32=>uint32');
            frameChannelCount = fread(fid, 1, 'uint32=>uint32');
            if isempty(pointCount) || isempty(frameChannelCount)
                break;
            end
            if frameChannelCount ~= channelCountExpected || pointCount <= 0
                warning('Invalid frame header in file: %s', filePath);
                break;
            end

            for c = 1:channelCountExpected
                channelValues = fread(fid, [double(pointCount), 1], 'double=>double');
                if numel(channelValues) ~= pointCount
                    warning('Incomplete frame data in file: %s', filePath);
                    stopThisFile = true;
                    break;
                end
                if selectedMask(c)
                    channels{c} = appendAndTrim(channels{c}, channelValues, maxReadPointsPerChannel);
                end
            end
            if stopThisFile
                break;
            end
            totalFrames = totalFrames + 1;
        end

        clear cleaner;
    end

    totalPoints = max(cellfun(@numel, channels));
    data = struct('channels', {channels});
    meta = struct('format', 'session_bin', ...
                  'channelCount', channelCountExpected, ...
                  'totalPointsPerChannel', totalPoints, ...
                  'frameCount', totalFrames, ...
                  'selectedChannels', selectedChannels, ...
                  'maxReadPointsPerChannel', maxReadPointsPerChannel, ...
                  'sourceFiles', {filePaths});
end

function [data, meta] = readLegacyChannelFiles(filePaths, selectedChannels, maxReadPointsPerChannel)
    channelCount = 16;
    channels = cell(1, channelCount);
    for c = 1:channelCount
        channels{c} = zeros(0, 1);
    end
    selectedMask = false(1, channelCount);
    selectedMask(selectedChannels) = true;

    for i = 1:numel(filePaths)
        filePath = filePaths{i};
        name = lower(getName(filePath));
        token = regexp(name, '^ch(\d+)\.bin$', 'tokens', 'once');
        if isempty(token)
            warning('Skip non-channel file: %s', filePath);
            continue;
        end

        ch = str2double(token{1});
        if isnan(ch) || ch < 1 || ch > channelCount
            warning('Invalid channel index in file name: %s', filePath);
            continue;
        end
        if ~selectedMask(ch)
            continue;
        end

        fid = fopen(filePath, 'rb', 'ieee-le');
        if fid < 0
            warning('Cannot open file: %s', filePath);
            continue;
        end
        cleaner = onCleanup(@() fclose(fid));

        if maxReadPointsPerChannel > 0
            fileInfo = dir(filePath);
            bytesPerPoint = 8;
            sampleCount = floor(double(fileInfo.bytes) / bytesPerPoint);
            readCount = min(sampleCount, double(maxReadPointsPerChannel));
            offsetBytes = max(0, fileInfo.bytes - readCount * bytesPerPoint);
            if fseek(fid, offsetBytes, 'bof') ~= 0
                warning('Seek failed: %s', filePath);
                clear cleaner;
                continue;
            end
        end

        values = fread(fid, inf, 'double=>double');
        channels{ch} = appendAndTrim(channels{ch}, values(:), maxReadPointsPerChannel);

        clear cleaner;
    end

    totalPoints = max(cellfun(@numel, channels));
    data = struct('channels', {channels});
    meta = struct('format', 'legacy_ch_bin', ...
                  'channelCount', channelCount, ...
                  'totalPointsPerChannel', totalPoints, ...
                  'frameCount', 0, ...
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

function freqInfo = detectChannelFrequencies(data, selectedChannels, maxPlotPoints, sampleRateHz)
    channelCount = numel(data.channels);
    freqInfo = repmat(struct('channel', 0, ...
                             'dominantHz', NaN, ...
                             'sampleCount', 0, ...
                             'status', "not_selected"), 1, channelCount);

    for ch = 1:channelCount
        freqInfo(ch).channel = ch;
    end

    for idx = 1:numel(selectedChannels)
        ch = selectedChannels(idx);
        y = getTailPoints(data.channels{ch}, maxPlotPoints);
        info = estimateDominantFrequency(y, sampleRateHz);
        info.channel = ch;
        freqInfo(ch) = info;
    end
end

function info = estimateDominantFrequency(y, sampleRateHz)
    y = double(y(:));
    y = y(isfinite(y));

    info = struct('channel', 0, ...
                  'dominantHz', NaN, ...
                  'sampleCount', numel(y), ...
                  'status', "unavailable");

    if sampleRateHz <= 0
        info.status = "disabled";
        return;
    end

    if numel(y) < 8
        info.status = "too_few_points";
        return;
    end

    y = y - mean(y);
    if max(y) - min(y) <= eps(max(abs(y)) + 1)
        info.status = "flat_signal";
        return;
    end

    n = numel(y);
    window = 0.5 - 0.5 * cos(2 * pi * (0:n-1)' / max(1, n - 1));
    spectrum = abs(fft(y .* window));
    magnitude = spectrum(1:floor(n / 2) + 1);

    if numel(magnitude) < 3
        info.status = "too_few_points";
        return;
    end

    magnitude(1) = 0;
    [peakValue, peakIndex] = max(magnitude);
    if ~isfinite(peakValue) || peakValue <= 0
        info.status = "no_peak";
        return;
    end

    peakOffset = 0;
    if peakIndex > 1 && peakIndex < numel(magnitude)
        left = magnitude(peakIndex - 1);
        center = magnitude(peakIndex);
        right = magnitude(peakIndex + 1);
        denominator = left - 2 * center + right;
        if isfinite(denominator) && abs(denominator) > eps(center)
            peakOffset = 0.5 * (left - right) / denominator;
            peakOffset = max(min(peakOffset, 1), -1);
        end
    end

    dominantHz = ((peakIndex - 1) + peakOffset) * sampleRateHz / n;
    if dominantHz <= 0 || dominantHz > sampleRateHz / 2
        info.status = "invalid_peak";
        return;
    end

    info.dominantHz = dominantHz;
    info.status = "ok";
end

function yTail = getTailPoints(y, maxPoints)
    y = y(:);
    if isempty(y)
        yTail = zeros(0, 1);
        return;
    end

    if maxPoints <= 0 || numel(y) <= maxPoints
        yTail = y;
        return;
    end

    yTail = y(end - maxPoints + 1:end);
end

function label = buildFrequencyLabel(info)
    if strcmp(info.status, "ok")
        label = sprintf('CH%d | Dominant Freq %.3f Hz', info.channel, info.dominantHz);
    else
        label = sprintf('CH%d | Dominant Freq N/A', info.channel);
    end
end

function printFrequencySummary(freqInfo, selectedChannels)
    fprintf('Frequency summary (based on plotted tail data):\n');
    for idx = 1:numel(selectedChannels)
        ch = selectedChannels(idx);
        info = freqInfo(ch);
        if strcmp(info.status, "ok")
            fprintf('  CH%d: %.3f Hz (points used: %d)\n', ch, info.dominantHz, info.sampleCount);
        else
            fprintf('  CH%d: N/A (%s)\n', ch, frequencyStatusToText(info.status));
        end
    end
end

function textValue = frequencyStatusToText(status)
    switch char(status)
        case 'ok'
            textValue = 'ok';
        case 'disabled'
            textValue = 'frequency detection disabled';
        case 'too_few_points'
            textValue = 'too few points';
        case 'flat_signal'
            textValue = 'flat signal';
        case 'no_peak'
            textValue = 'no spectral peak found';
        case 'invalid_peak'
            textValue = 'invalid frequency peak';
        otherwise
            textValue = 'frequency unavailable';
    end
end

function plotDsaChannels(data, meta, selectedChannels, maxPlotPoints, freqInfo, sampleRateHz)
    figure('Color', 'w', 'Name', 'DSA 16CH Plot', 'NumberTitle', 'off');
    nPlots = numel(selectedChannels);
    topTitle = sprintf('Format: %s | Fs: %.3f Hz | Points/Ch(max): %d', ...
                       meta.format, sampleRateHz, meta.totalPointsPerChannel);

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
        yPlot = getTailPoints(y, maxPlotPoints);

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
        channelTitle = buildFrequencyLabel(freqInfo(ch));
        if ~hasTiledLayout && ~hasSgTitle && ~hasSupTitle && k == 1
            title(sprintf('%s\n%s', topTitle, channelTitle), 'Interpreter', 'none');
        else
            title(channelTitle, 'Interpreter', 'none');
        end
        if k == nPlots
            xlabel('Sample Index (tail)');
        end

        % Very old MATLAB fallback when neither sgtitle nor suptitle exists.
    end
end

function name = getName(filePath)
    [~, name, ext] = fileparts(filePath);
    name = [name ext];
end
