import SwiftUI

extension Color {
    static let skiffBackground = Color(red: 10.0 / 255.0, green: 14.0 / 255.0, blue: 23.0 / 255.0)
    static let skiffSurface = Color(red: 17.0 / 255.0, green: 23.0 / 255.0, blue: 38.0 / 255.0)
    static let skiffSurfaceAlt = Color(red: 23.0 / 255.0, green: 31.0 / 255.0, blue: 49.0 / 255.0)
    static let skiffAccent = Color(red: 110.0 / 255.0, green: 168.0 / 255.0, blue: 255.0 / 255.0)
    static let skiffViolet = Color(red: 155.0 / 255.0, green: 109.0 / 255.0, blue: 255.0 / 255.0)
    static let skiffText = Color(red: 232.0 / 255.0, green: 237.0 / 255.0, blue: 246.0 / 255.0)
    static let skiffMuted = Color(red: 147.0 / 255.0, green: 161.0 / 255.0, blue: 184.0 / 255.0)
}

extension LinearGradient {
    static let skiffAccent = LinearGradient(
        colors: [Color.skiffAccent, Color.skiffViolet],
        startPoint: .topLeading,
        endPoint: .bottomTrailing
    )
}

extension View {
    func skiffCardBackground() -> some View {
        background(RoundedRectangle(cornerRadius: 14, style: .continuous).fill(Color.skiffSurface))
    }
}
