//
//  Result+Conveniences.swift
//  AltStore
//
//  Created by Riley Testut on 5/22/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

import Foundation

extension Result
{
    var value: Success? {
        switch self
        {
        case .success(let value): return value
        case .failure: return nil
        }
    }
    
    var error: Failure? {
        switch self
        {
        case .success: return nil
        case .failure(let error): return error
        }
    }
    
    init(_ value: Success?, _ error: Failure?)
    {
        switch (value, error)
        {
        case (let value?, _): self = .success(value)
        case (_, let error?): self = .failure(error)
        case (nil, nil): preconditionFailure("Either value or error must be non-nil")
        }
    }
}

extension Result where Success == Void
{
    init(_ success: Bool, _ error: Failure?)
    {
        if success
        {
            self = .success(())
        }
        else if let error = error
        {
            self = .failure(error)
        }
        else
        {
            preconditionFailure("Error must be non-nil if success is false")
        }
    }
}

extension Result
{
    init<T, U>(_ values: (T?, U?), _ error: Failure?) where Success == (T, U)
    {
        if let value1 = values.0, let value2 = values.1
        {
            self = .success((value1, value2))
        }
        else if let error = error
        {
            self = .failure(error)
        }
        else
        {
            preconditionFailure("Error must be non-nil if either provided values are nil")
        }
    }
}
