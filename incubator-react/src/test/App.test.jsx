import {describe, it, expect} from 'vitest'
import {render, screen} from '@testing-library/react'
import App from '../App'

describe('Incubator Dashboard', () => {
    it('Dashboard title visible', () => {
        render(<App/>)
        expect(screen.getByText(/Incubator Dashboard/)).toBeInTheDocument()
    }), it('Dashboard version visible', () => {
        render(<App/>)
        expect(screen.getByText(/Version:/i)).toBeInTheDocument()
    })
})