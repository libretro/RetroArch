//
//  CodableServerError.swift
//  AltKit
//
//  Created by Riley Testut on 3/5/20.
//  Copyright © 2020 Riley Testut. All rights reserved.
//

import Foundation

// Can only automatically conform ALTServerError.Code to Codable, not ALTServerError itself
extension ALTServerError.Code: Codable {}

extension CodableServerError
{
    enum UserInfoValue: Codable
    {
        case string(String)
        case error(NSError)
        
        public init(from decoder: Decoder) throws
        {
            let container = try decoder.singleValueContainer()

            if
                let data = try? container.decode(Data.self),
                let error = try? NSKeyedUnarchiver.unarchivedObject(ofClass: NSError.self, from: data)
            {
                self = .error(error)
            }
            else if let string = try? container.decode(String.self)
            {
                self = .string(string)
            }
            else
            {
                throw DecodingError.dataCorruptedError(in: container, debugDescription: "UserInfoValue value cannot be decoded")
            }
        }
        
        func encode(to encoder: Encoder) throws
        {
            var container = encoder.singleValueContainer()
            
            switch self
            {
            case .string(let string): try container.encode(string)
            case .error(let error):
                guard let data = try? NSKeyedArchiver.archivedData(withRootObject: error, requiringSecureCoding: true) else {
                    let context = EncodingError.Context(codingPath: container.codingPath, debugDescription: "UserInfoValue value \(self) cannot be encoded")
                    throw EncodingError.invalidValue(self, context)
                }
                
                try container.encode(data)
            }
        }
    }
}

struct CodableServerError: Codable
{
    var error: ALTServerError {
        return ALTServerError(self.errorCode, userInfo: self.userInfo ?? [:])
    }
    
    private var errorCode: ALTServerError.Code
    private var userInfo: [String: Any]?
    
    private enum CodingKeys: String, CodingKey
    {
        case errorCode
        case userInfo
    }

    init(error: ALTServerError)
    {
        self.errorCode = error.code
        
        var userInfo = error.userInfo
        if let localizedRecoverySuggestion = (error as NSError).localizedRecoverySuggestion
        {
            userInfo[NSLocalizedRecoverySuggestionErrorKey] = localizedRecoverySuggestion
        }
        
        if !userInfo.isEmpty
        {
            self.userInfo = userInfo
        }
    }
    
    init(from decoder: Decoder) throws
    {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        
        let errorCode = try container.decode(Int.self, forKey: .errorCode)
        self.errorCode = ALTServerError.Code(rawValue: errorCode) ?? .unknown
        
        let rawUserInfo = try container.decodeIfPresent([String: UserInfoValue].self, forKey: .userInfo)
        
        let userInfo = rawUserInfo?.mapValues { (value) -> Any in
            switch value
            {
            case .string(let string): return string
            case .error(let error): return error
            }
        }
        self.userInfo = userInfo
    }
    
    func encode(to encoder: Encoder) throws
    {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(self.error.code.rawValue, forKey: .errorCode)
        
        let rawUserInfo = self.userInfo?.compactMapValues { (value) -> UserInfoValue? in
            switch value
            {
            case let string as String: return .string(string)
            case let error as NSError: return .error(error)
            default: return nil
            }
        }
        try container.encodeIfPresent(rawUserInfo, forKey: .userInfo)
    }
}

