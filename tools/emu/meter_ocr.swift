// Prints the text macOS Vision reads in each image: "file<TAB>line | line ...".
// Built and used by tools/emu/read_meter.py.
import Foundation
import Vision
import AppKit
for path in CommandLine.arguments.dropFirst() {
  guard let img = NSImage(contentsOfFile: path),
        let cg = img.cgImage(forProposedRect: nil, context: nil, hints: nil) else { continue }
  let req = VNRecognizeTextRequest()
  req.recognitionLevel = .accurate
  try? VNImageRequestHandler(cgImage: cg).perform([req])
  let lines = (req.results ?? []).compactMap { $0.topCandidates(1).first?.string }
  print((path as NSString).lastPathComponent + "\t" + lines.joined(separator: " | "))
}
