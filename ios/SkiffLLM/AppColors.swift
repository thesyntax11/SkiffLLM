import SwiftUI
import UIKit

extension Color {
    private static func adapt(light: UIColor, dark: UIColor) -> Color {
        Color(UIColor { trait in
            trait.userInterfaceStyle == .dark ? dark : light
        })
    }

    static let skiffBackground = adapt(
        light: UIColor(red: 246.0 / 255.0, green: 244.0 / 255.0, blue: 240.0 / 255.0, alpha: 1),
        dark: UIColor(red: 20.0 / 255.0, green: 18.0 / 255.0, blue: 16.0 / 255.0, alpha: 1)
    )
    static let skiffSurface = adapt(
        light: UIColor(red: 1.0, green: 1.0, blue: 1.0, alpha: 1),
        dark: UIColor(red: 29.0 / 255.0, green: 26.0 / 255.0, blue: 23.0 / 255.0, alpha: 1)
    )
    static let skiffSurfaceAlt = adapt(
        light: UIColor(red: 239.0 / 255.0, green: 236.0 / 255.0, blue: 229.0 / 255.0, alpha: 1),
        dark: UIColor(red: 49.0 / 255.0, green: 44.0 / 255.0, blue: 38.0 / 255.0, alpha: 1)
    )
    static let skiffAccent = adapt(
        light: UIColor(red: 196.0 / 255.0, green: 85.0 / 255.0, blue: 45.0 / 255.0, alpha: 1),
        dark: UIColor(red: 217.0 / 255.0, green: 121.0 / 255.0, blue: 82.0 / 255.0, alpha: 1)
    )
    static let skiffAccentForeground = adapt(
        light: UIColor.white,
        dark: UIColor(red: 28.0 / 255.0, green: 19.0 / 255.0, blue: 14.0 / 255.0, alpha: 1)
    )
    static let skiffText = adapt(
        light: UIColor(red: 33.0 / 255.0, green: 29.0 / 255.0, blue: 25.0 / 255.0, alpha: 1),
        dark: UIColor(red: 239.0 / 255.0, green: 233.0 / 255.0, blue: 223.0 / 255.0, alpha: 1)
    )
    static let skiffMuted = adapt(
        light: UIColor(red: 111.0 / 255.0, green: 102.0 / 255.0, blue: 92.0 / 255.0, alpha: 1),
        dark: UIColor(red: 167.0 / 255.0, green: 157.0 / 255.0, blue: 144.0 / 255.0, alpha: 1)
    )
}

extension View {
    func skiffCardBackground() -> some View {
        background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(Color.skiffSurface))
    }
}
