#include "appleshare.h"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <QCoreApplication>
#include <QList>
#include <QUrl>

#include <cstdio>
#include <functional>
#include <utility>

namespace {
using FileSelection = std::function<void(const QString &)>;

NSWindow *activeWindow()
{
    NSWindow *window = NSApp.keyWindow;
    return window ? window : NSApp.mainWindow;
}

void presentFilePanel(NSSavePanel *panel, FileSelection selected)
{
    // AppKit owns the active panel until the completion handler returns.
    void (^completed)(NSModalResponse) = ^(NSModalResponse response) {
        const QString path = response == NSModalResponseOK && panel.URL
            ? QString::fromNSString(panel.URL.path) : QString{};
        selected(path);
    };
    if (NSWindow *window = activeWindow())
        [panel beginSheetModalForWindow:window completionHandler:completed];
    else
        [panel beginWithCompletionHandler:completed];
}
}

namespace appleShareMac {
void choosePdfDestination(const QString &fileName, FileSelection selected)
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = QCoreApplication::translate("AppleShareMac", "Salva PDF").toNSString();
    panel.nameFieldStringValue = fileName.toNSString();
    panel.allowedContentTypes = @[UTTypePDF];
    panel.canCreateDirectories = YES;
    presentFilePanel(panel, std::move(selected));
}

void chooseNotebookFile(FileSelection selected)
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = QCoreApplication::translate("AppleShareMac", "Importa quaderni").toNSString();
    UTType *quaderno = [UTType typeWithFilenameExtension:@"quaderno"];
    panel.allowedContentTypes = quaderno ? @[UTTypeJSON, quaderno] : @[UTTypeJSON];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    presentFilePanel(panel, std::move(selected));
}

void openAppleBooks(std::function<void(bool)> completed)
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *url = [workspace URLForApplicationWithBundleIdentifier:@"com.apple.iBooksX"];
    if (!url) {
        completed(false);
        return;
    }
    [workspace openApplicationAtURL:url configuration:[NSWorkspaceOpenConfiguration configuration]
                  completionHandler:^(NSRunningApplication *app, NSError *error) {
        const bool opened = app != nil && error == nil;
        dispatch_async(dispatch_get_main_queue(), ^{ completed(opened); });
    }];
}

void chooseAppleBooksFiles(std::function<void(const QList<QUrl> &)> selected)
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = QCoreApplication::translate("AppleShareMac", "Importa da Apple Books").toNSString();
    NSMutableArray<UTType *> *types = [NSMutableArray arrayWithArray:@[UTTypePDF, UTTypeEPUB]];
    // Finder can represent an exported EPUB as a single selectable package.
    for (NSString *identifier in @[@"org.idpf.epub-folder", @"com.apple.ibooks.epub"]) {
        if (UTType *type = [UTType typeWithIdentifier:identifier])
            [types addObject:type];
    }
    panel.allowedContentTypes = types;
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.treatsFilePackagesAsDirectories = NO;
    panel.allowsMultipleSelection = YES;

    void (^completed)(NSModalResponse) = ^(NSModalResponse response) {
        QList<QUrl> files;
        if (response == NSModalResponseOK) {
            for (NSURL *url in panel.URLs) {
                if (url.isFileURL)
                    files.append(QUrl::fromLocalFile(QString::fromNSString(url.path)));
            }
        }
        selected(files);
    };
    if (NSWindow *window = activeWindow())
        [panel beginSheetModalForWindow:window completionHandler:completed];
    else
        [panel beginWithCompletionHandler:completed];
}

bool sharePdf(const QString &path)
{
    NSURL *url = [NSURL fileURLWithPath:path.toNSString()];
    NSWindow *window = activeWindow();
    NSView *view = window.contentView;
    if (!view) {
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
        return false;
    }

    static NSSharingServicePicker *picker = nil;
    if (@available(macOS 13.0, *))
        [picker close];
    picker = [[NSSharingServicePicker alloc] initWithItems:@[url]];

    NSPoint point = [view convertPoint:window.mouseLocationOutsideOfEventStream fromView:nil];
    if (!NSPointInRect(point, view.bounds))
        point = NSMakePoint(NSMidX(view.bounds), NSMidY(view.bounds));
    [picker showRelativeToRect:NSMakeRect(point.x, point.y, 1, 1)
                       ofView:view
                preferredEdge:NSMinYEdge];
    return true;
}

void revealExport(const QString &path)
{
    [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:path.toNSString()]]];
}

void openExport(const QString &path, const QString &destination, std::function<void(bool)> completed)
{
    if (destination == QStringLiteral("share") || destination == QStringLiteral("journal")) {
        // Showing the picker cannot prove that the user delivered the file.
        // The durable queue remains awaiting user confirmation, including cancel.
        completed(sharePdf(path));
        return;
    }
    NSString *bundle = nil;
    if (destination == QStringLiteral("notes")) bundle = @"com.apple.Notes";
    else if (destination == QStringLiteral("books")) bundle = @"com.apple.iBooksX";
    else if (destination == QStringLiteral("numbers")) bundle = @"com.apple.iWork.Numbers";
    else if (destination == QStringLiteral("keynote")) bundle = @"com.apple.iWork.Keynote";
    if (!bundle) { completed(false); return; }
    NSWorkspace *workspace = NSWorkspace.sharedWorkspace;
    NSURL *application = [workspace URLForApplicationWithBundleIdentifier:bundle];
    if (!application) { completed(false); return; }
    NSURL *file = [NSURL fileURLWithPath:path.toNSString()];
    const bool manualImport = destination == QStringLiteral("notes") || destination == QStringLiteral("keynote");
    if (manualImport) {
        [workspace activateFileViewerSelectingURLs:@[file]];
        [workspace openApplicationAtURL:application configuration:[NSWorkspaceOpenConfiguration configuration]
                      completionHandler:^(NSRunningApplication *app, NSError *error) {
            const bool opened = app != nil && error == nil;
            dispatch_async(dispatch_get_main_queue(), ^{ completed(opened); });
        }];
    } else {
        [workspace openURLs:@[file] withApplicationAtURL:application
              configuration:[NSWorkspaceOpenConfiguration configuration]
          completionHandler:^(NSRunningApplication *app, NSError *error) {
            const bool opened = app != nil && error == nil;
            dispatch_async(dispatch_get_main_queue(), ^{ completed(opened); });
        }];
    }
}
}

int runSshAskpass(const QString &prompt)
{
    if (qEnvironmentVariable("SSH_ASKPASS_PROMPT") == QStringLiteral("confirm"))
        return 1;

    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        if (@available(macOS 14.0, *))
            [app activate];
        else
            [app activateIgnoringOtherApps:YES];

        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = QCoreApplication::translate("AppleShareMac", "Accesso al Paper Pro").toNSString();
        alert.informativeText = prompt.simplified().left(256).toNSString();
        [alert addButtonWithTitle:QCoreApplication::translate("AppleShareMac", "Connetti").toNSString()];
        NSButton *cancel = [alert addButtonWithTitle:QCoreApplication::translate("AppleShareMac", "Annulla").toNSString()];
        cancel.keyEquivalent = @"\033";

        NSSecureTextField *field = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
        field.placeholderString = QCoreApplication::translate("AppleShareMac", "Password SSH").toNSString();
        alert.accessoryView = field;
        alert.window.initialFirstResponder = field;

        if ([alert runModal] != NSAlertFirstButtonReturn)
            return 1;

        NSString *password = field.stringValue;
        field.stringValue = @"";
        if (!password.length || [password rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location != NSNotFound)
            return 1;

        NSData *encoded = [password dataUsingEncoding:NSUTF8StringEncoding];
        if (!encoded.length || std::fwrite(encoded.bytes, 1, encoded.length, stdout) != encoded.length
            || std::fputc('\n', stdout) == EOF || std::fflush(stdout) != 0)
            return 1;
        return 0;
    }
}
