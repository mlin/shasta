#ifndef SHASTA_STATIC_EXECUTABLE

// Shasta.
#include "LocalMarkerGraph.hpp"
#include "ConsensusCaller.hpp"
#include "Marker.hpp"
#include "MemoryMappedVectorOfVectors.hpp"
using namespace ChanZuckerberg;
using namespace shasta;

// Boost libraries.
#include <boost/graph/graphviz.hpp>

// Standard libraries.
#include "fstream.hpp"



// Write the graph in Graphviz format.
void LocalMarkerGraph::write(
    const string& fileName,
    size_t minCoverage,
    int maxDistance,
    bool detailed,
    bool showVertexId) const
{
    ofstream outputFileStream(fileName);
    if(!outputFileStream) {
        throw runtime_error("Error opening " + fileName);
    }
    write(outputFileStream, minCoverage, maxDistance, detailed, showVertexId);
}
void LocalMarkerGraph::write(
    ostream& s,
    size_t minCoverage,
    int maxDistance,
    bool detailed,
    bool showVertexId) const
{
    Writer writer(*this, minCoverage, maxDistance, detailed, showVertexId);
    boost::write_graphviz(s, *this, writer, writer, writer,
        boost::get(&LocalMarkerGraphVertex::vertexId, *this));
}

LocalMarkerGraph::Writer::Writer(
    const LocalMarkerGraph& graph,
    size_t minCoverage,
    int maxDistance,
    bool detailed,
    bool showVertexId) :
    graph(graph),
    minCoverage(minCoverage),
    maxDistance(maxDistance),
    detailed(detailed),
    showVertexId(showVertexId)
{
}



void LocalMarkerGraph::Writer::operator()(std::ostream& s) const
{
    // This turns off the tooltip on the graph and the edges.
    s << "tooltip = \" \";\n";

    if(detailed) {
        s << "layout=dot;\n";
        s << "rankdir=LR;\n";
        s << "ratio=expand;\n";
        s << "node [fontname = \"Courier New\" shape=rectangle];\n";
        s << "edge [fontname = \"Courier New\"];\n";
    } else {
        s << "layout=sfdp;\n";
        s << "smoothing=triangle;\n";
        s << "ratio=expand;\n";
        s << "node [shape=point];\n";
    }
}



void LocalMarkerGraph::Writer::operator()(std::ostream& s, vertex_descriptor v) const
{
    const LocalMarkerGraphVertex& vertex = graph[v];
    const auto coverage = vertex.markerInfos.size();
    CZI_ASSERT(coverage > 0);


    // For compact output, the node shape is already defaulted to point,
    // and we don't write a label. The tooltip contains the vertex id,
    // which can be used to create a local subgraph to be looked at
    // in detailed format (use scripts/CreateLocalSubgraph.py).
    if(!detailed) {

        // Compact output.

        // Begin vertex attributes.
        s << "[";

        // Id, so we can use JavaScript code to manipulate the vertex.
        s << "id=vertex" << vertex.vertexId;

        // Tooltip.
        s << " tooltip=\"";
        if(showVertexId) {
            s << "Vertex " << vertex.vertexId << ", coverage ";
        } else {
            s << "Coverage ";
        }
        s << coverage << ", distance " << vertex.distance << ", rank " << vertex.rank;
        s << ", click to recenter graph here, right click for detail\"";

        // Vertex size.
        s << " width=\"";
        const auto oldPrecision = s.precision(4);
        s << 0.05 * sqrt(double(coverage));
        s.precision(oldPrecision);
        s << "\"";

        // Color.
        string color;
        if(vertex.distance == maxDistance) {
            color = "cyan";
        } else if(vertex.distance == 0) {
            color = "#90ee90";
        } else  if(coverage >= minCoverage) {
            color = "black";
        } else {
            color = "red";
        }
        s << " fillcolor=\"" << color << "\" color=\"" << color << "\"";

        // End vertex attributes.
        s << "]";

    } else {

        // Detailed output.
        const size_t k = graph.k;
        const KmerId kmerId = graph.getKmerId(v);
        const Kmer kmer(kmerId, k);

        // Begin vertex attributes.
        s << "[";

        // Color.
        string color;
        if(vertex.distance == maxDistance) {
            color = "cyan";
        } else if(vertex.distance == 0) {
            color = "#90ee90";
        } else if(coverage >= minCoverage) {
            color = "green";
        } else {
            color = "red";
        }
        s << " style=filled";
        s << " fillcolor=\"" << color << "\"";

        // Id, so we can use JavaScript code to manipulate the vertex.
        s << " id=vertex" << vertex.vertexId;

        // Tooltip.
        s << " tooltip=\"";
        if(showVertexId) {
            s << "Vertex " << vertex.vertexId << ", coverage ";
        } else {
            s << "Coverage ";
        }
        s << coverage << ", distance " << vertex.distance << ", rank "  << vertex.rank << "\"";

        // Write the label using Graphviz html-like functionality.
        s << " label=<<font><table border=\"0\">";
        const int columnCount = 4;

        // Vertex id.
        if(showVertexId) {
            s << "<tr><td colspan=\"" << columnCount << "\"><b>";
            s << "Vertex " << vertex.vertexId;
            s << "</b></td></tr>";
        }

        // Kmer.
        s << "<tr><td colspan=\"" << columnCount << "\"><b>";
        kmer.write(s, k);
        s << "</b></td></tr>";

        // Coverage.
        s << "<tr><td colspan=\"" << columnCount << "\"><b>";
        s << "Coverage " << coverage;
        s << "</b></td></tr>";

        // Distance.
        s << "<tr><td colspan=\"" << columnCount << "\" ";
        s << " href=\"\"";  // Necessary to activate tooltip.
        s << " id=\"vertexDistance" << vertex.vertexId << "\" tooltip=\"Click to recenter graph here\">";
        s << "<font color=\"blue\"><b><u>Distance " << vertex.distance;
        s << "</u></b></font></td></tr>";

        // Rank.
        s << "<tr><td colspan=\"" << columnCount << "\">";
        s << "<b>Rank " << vertex.rank;
        s << "</b></td></tr>";

        // Column headers.
        s << "<tr><td><b>Read</b></td><td><b>Ord</b></td><td><b>Pos</b></td>";
        s << "<td><b>Repeat</b></td>";
        s << "</tr>";

        // A row for each marker of this vertex.
        for(const auto& markerInfo: vertex.markerInfos) {
            const CompressedMarker& marker = graph.markers.begin()[markerInfo.markerId];

            // OrientedReadId
            s << "<tr><td align=\"right\"";
            s << " href=\"exploreRead?readId&amp;" << markerInfo.orientedReadId.getReadId();
            s << "&amp;strand=" << markerInfo.orientedReadId.getStrand() << "\"";
            s << "><font color=\"blue\"><b><u>" << markerInfo.orientedReadId << "</u></b></font></td>";

            // Ordinal.
            s << "<td align=\"right\"";
            s << " href=\"exploreRead?readId=" << markerInfo.orientedReadId.getReadId();
            s << "&amp;strand=" << markerInfo.orientedReadId.getStrand();
            s << "&amp;highlightMarker=" << markerInfo.ordinal;
            s << "\"";
            s << "><font color=\"blue\"><b><u>" << markerInfo.ordinal << "</u></b></font></td>";

            // Position.
            s << "<td align=\"right\"><b>" << marker.position << "</b></td>";

            // Repeat counts.
            const vector<uint8_t> counts = graph.getRepeatCounts(markerInfo);
            s << "<td><b>";
            for(size_t i=0; i<k; i++) {
                if(counts[i] < 10) {
                    s << int(counts[i]);
                } else {
                    s << "*";
                }
            }
            s << "</b></td>";

            s << "</tr>";
        }



        // Repeat count consensus.

        // Use the consensus caller to compute the consensus base and repeat count
        // at each of the k positions. The consensus base should be equal
        // to the corresponding base of the k-mer for this vertex!
        vector<Consensus> consensus(k);
        for(size_t position=0; position<graph.k; position++) {
            consensus[position] = graph.consensusCaller(vertex.coverages[position]);
            CZI_ASSERT(consensus[position].base == AlignedBase(kmer[position]));
        }

        s << "<tr><td colspan=\"3\" align=\"left\"><b>Repeat consensus</b></td>";
        s << "<td><b>";
        for(size_t position=0; position<graph.k; position++) {
            const size_t repeatCount = consensus[position].repeatCount;
            if(repeatCount < 10) {
                s << repeatCount;
            } else {
                s << "*";
            }
        }
        s << "</b></td></tr>";

        // Coverage for each repeat count at each position.
        const std::set<size_t> repeatCounts =
            graph.consensusCaller.findRepeatCounts(vertex.coverages);
        for(const size_t repeatCount: repeatCounts) {
            s << "<tr>";
            s << "<td colspan=\"3\" align=\"left\"><b>Coverage for repeat ";
            s << repeatCount << "</b></td>";
            s << "<td><b>";
            for(size_t position=0; position<graph.k; position++) {
                const AlignedBase base = AlignedBase(kmer[position]);
                s << vertex.coverages[position].coverageCharacter(base, repeatCount);
            }
            s << "</b></td></tr>";
        }

        // Coverage for the consensus best repeat count at each position.
        s << "<tr><td colspan=\"3\" align=\"left\"><b>Coverage for repeat consensus</b></td>";
        s << "<td><b>";
        for(size_t position=0; position<graph.k; position++) {
            const AlignedBase base = AlignedBase(kmer[position]);
            const size_t repeatCount = consensus[position].repeatCount;
            s << vertex.coverages[position].coverageCharacter(base, repeatCount);
        }
        s << "</b></td></tr>";

        // The raw sequence, based on the best repeat counts.
        s << "<tr><td colspan=\"3\" align=\"left\"><b>Raw consensus</b></td>";
        s << "<td align=\"left\"><b>";
        for(size_t position=0; position<graph.k; position++) {
            const AlignedBase base = AlignedBase(kmer[position]);
            const size_t repeatCount = consensus[position].repeatCount;
            for(size_t k=0; k<repeatCount; k++) {
                s << base;
        }
        }
        s << "</b></td></tr>";



        // End the table.
        s << "</table></font>>";

        // End vertex attributes.
        s << "]";
    }
}



void LocalMarkerGraph::Writer::operator()(std::ostream& s, edge_descriptor e) const
{

    const LocalMarkerGraphEdge& edge = graph[e];
    const size_t coverage = edge.coverage();
    CZI_ASSERT(coverage > 0);
    const size_t consensus = edge.consensus();

    if(!detailed) {

        // Compact output.

        // Begin edge attributes.
        s << "[";

        // Tooltip.
        s << "tooltip=\"Coverage " << coverage << ", consensus " << consensus << "\"";

        // Color.
        string color;
        if(edge.isSpanningTreeEdge) {
            color = "violet";
        } else if(coverage >= minCoverage) {
            color = "black";
        } else {
            color = "red";
        }
        s << " fillcolor=\"" << color << "\"";
        s << " color=\"" << color << "\"";

        // Thickness is determined by coverage.
        const double thickness = 0.2 * double(coverage==0 ? 1 : coverage);
        s << " penwidth=";
        const auto oldPrecision = s.precision(4);
        s <<  thickness;
        s.precision(oldPrecision);

        // Style.
        if(edge.isSpanningTreeEdge && !edge.isSpanningTreeBestPathEdge) {
            s << " style=dashed";
        }

        // Weight;
        s << " weight=" << coverage;

        // End edge attributes.
        s << "]";

    } else {

        // Detailed output.

        // Begin edge attributes.
        s << "[";

        const string tooltipText = "Coverage " + to_string(coverage) + ", consensus " +to_string(consensus);
        s << " tooltip=\"" << tooltipText << "\"";
        s << " labeltooltip=\"" << tooltipText << "\"";
        // s << " URL=\"#abcdef\"";   // Hack to convince graphviz to not ignore the labeltooltip.

        // Thickness is determined by coverage.
        const double thickness = 0.5 * double(coverage==0 ? 1 : coverage);
        s << " penwidth=";
        const auto oldPrecision = s.precision(4);
        s <<  thickness;
        s.precision(oldPrecision);

        // Style.
        if(edge.isSpanningTreeEdge && !edge.isSpanningTreeBestPathEdge) {
            s << " style=dashed";
        }

        // Color.
        string color;
        if(edge.isSpanningTreeEdge) {
            color = "violet";
        } else if(coverage >= minCoverage) {
            color = "black";
        } else {
            color = "red";
        }
        s << " fillcolor=\"" << color << "\"";
        s << " color=\"" << color << "\"";

        // Label color (used below).
        string labelColor;
        if(color == "black") {
            labelColor = "green";
        } else {
            labelColor = color;
        }


        // Weight;
        s << " weight=" << coverage;

        // If the edge was not marked as a DAG edge during approximate topological sort,
        // tell graphviz not to use it in constraint assignment.
        if(!edge.isDagEdge) {
            s << " constraint=false";
        }

        // Label.
        s << " label=<<font color=\"black\">";
        s << "<table";
        s << " color=\"black\"";
        s << " bgcolor=\"" << labelColor << "\"";
        s << " border=\"0\"";
        s << " cellborder=\"1\"";
        s << " cellspacing=\"1\"";
        s << ">";

        // Edge id.
        const int columnCount = 5;
        if(showVertexId && (edge.edgeId != MarkerGraph::invalidEdgeId)) {
            s << "<tr><td colspan=\"" << columnCount << "\"><b>Edge " << edge.edgeId << "</b></td></tr>";
        }

        // Assembly vertex id.
        if(showVertexId && (edge.assemblyEdgeId != std::numeric_limits<AssemblyGraph::VertexId>::max())) {
            s << "<tr><td colspan=\"" << columnCount << "\"><b>Position " << edge.positionInAssemblyEdge <<
                " in assembly graph edge " << edge.assemblyEdgeId << "</b></td></tr>";
        }

        // Consensus and coverage.
        s << "<tr><td colspan=\"" << columnCount << "\"><b>Coverage " << coverage << "</b></td></tr>";
        s << "<tr><td colspan=\"" << columnCount << "\"><b>Consensus " << consensus << "</b></td></tr>";

        // Header row.
        s <<
            "<tr>"
            "<td align=\"center\"><b>Read</b></td>"
            "<td align=\"center\"><b>Ord0</b></td>"
            "<td align=\"center\"><b>Ord1</b></td>"
            "<td align=\"center\"><b>Seq</b></td>";
        s << "<td align=\"center\"><b>Repeat</b></td>";
        s << "</tr>";

        // Loop over the infos table for this edge.
        for(const auto& p: edge.infos) {
            const auto& sequence = p.first;
            const auto& infos = p.second;

            // Construct the string representing this sequence.
            string sequenceString;
            if(sequence.sequence.empty()) {
                sequenceString = to_string(sequence.overlappingBaseCount);
            } else {
                for(const Base base: sequence.sequence) {
                    sequenceString.push_back(base.character());
                }
            }



            for(auto it=infos.begin(); it!=infos.end(); ++it) {
                const auto& info = *it;
                s << "<tr><td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << info.orientedReadId.getReadId();
                s << "&amp;strand=" << info.orientedReadId.getStrand() << "\"";
                s << "><font color=\"blue\"><b><u>" << info.orientedReadId << "</u></b></font></td>";

                s << "<td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << info.orientedReadId.getReadId();
                s << "&amp;strand=" << info.orientedReadId.getStrand();
                s << "&amp;highlightMarker=" << info.ordinals[0];
                s << "&amp;highlightMarker=" << info.ordinals[1];
                s << "\"";
                s << "><font color=\"blue\"><b><u>" << info.ordinals[0] << "</u></b></font></td>";

                s << "<td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << info.orientedReadId.getReadId();
                s << "&amp;strand=" << info.orientedReadId.getStrand();
                s << "&amp;highlightMarker=" << info.ordinals[0];
                s << "&amp;highlightMarker=" << info.ordinals[1];
                s << "\"";
                s << "><font color=\"blue\"><b><u>" << info.ordinals[1] << "</u></b></font></td>";

                s << "<td align=\"center\"><b>";
                if(it == infos.begin()) {
                    if(sequenceString.size() > 100) {
                        s << "Too long";
                    } else {
                        s << sequenceString;
                    }
                } else {
                    s << "=";
                }
                s << "</b></td>";

                // Write out the repeat counts, if necessary.
                if(!info.repeatCounts.empty()) {
                    s << "<td align=\"center\"><b>";
                    if(sequenceString.size() > 100) {
                        s << "Too long";
                    } else {
                        for(const uint8_t repeatCount: info.repeatCounts) {
                            if(repeatCount < 10) {
                                s << int(repeatCount);
                            } else {
                                s << "*";
                            }
                        }
                    }
                    s << "</b></td>";
                }

                s << "</tr>";
            }
        }



        // If the SeqAn alignment was computed, also write it to the table.
        if(edge.seqanAlignmentWasComputed) {
            s << "<tr><td colspan=\"" << columnCount << "\"><b>SeqAn alignment</b></td></tr>";

            // Add one row to the table for each read.
            for(size_t i=0; i<edge.alignmentInfos.size(); i++) {
                const auto& alignmentInfo = edge.alignmentInfos[i];

                // Begin a new row of the table.
                s << "<tr>";

                // Read id and ordinals.
                s << "<td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << alignmentInfo.orientedReadId.getReadId();
                s << "&amp;strand=" << alignmentInfo.orientedReadId.getStrand() << "\"";
                s << "><font color=\"blue\"><b><u>" << alignmentInfo.orientedReadId << "</u></b></font></td>";

                s << "<td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << alignmentInfo.orientedReadId.getReadId();
                s << "&amp;strand=" << alignmentInfo.orientedReadId.getStrand();
                s << "&amp;highlightMarker=" << alignmentInfo.ordinals[0];
                s << "&amp;highlightMarker=" << alignmentInfo.ordinals[1];
                s << "\"";
                s << "><font color=\"blue\"><b><u>" << alignmentInfo.ordinals[0] << "</u></b></font></td>";

                s << "<td align=\"right\"";
                s << " href=\"exploreRead?readId&amp;" << alignmentInfo.orientedReadId.getReadId();
                s << "&amp;strand=" << alignmentInfo.orientedReadId.getStrand();
                s << "&amp;highlightMarker=" << alignmentInfo.ordinals[0];
                s << "&amp;highlightMarker=" << alignmentInfo.ordinals[1];
                s << "\"";
                s << "><font color=\"blue\"><b><u>" << alignmentInfo.ordinals[1] << "</u></b></font></td>";

                // SeqAn alignment.
                const seqan::Gaps< seqan::String<seqan::Dna> >& alignmentRow =
                    seqan::row(edge.seqanAlignment, i);
                s << "<td><b>" << alignmentRow << "</b></td>";

                // Repeat counts on SeqAn alignment.
                s << "<td><b>";
                const auto n = seqan::length(alignmentRow);
                size_t position = 0;
                for(size_t j=0; j<n; j++) {
                    if(seqan::isGap(alignmentRow, j)) {
                        s << "-";
                    } else {
                        const int repeatCount = edge.alignmentInfos[i].repeatCounts[position++];
                        if(repeatCount < 10) {
                            s << repeatCount;
                        } else {
                            s << "*";
                        }
                    }
                }
                s << "</b></td>";

                // End this row of the table.
                s << "</tr>";
            }

            // Use the Consensus Caller to compute consensus
            // for base and repeat count at each position in the alignment.
            vector<Consensus> consensus(edge.coverages.size());
            for(size_t position=0; position<edge.coverages.size(); position++) {
                consensus[position] = graph.consensusCaller(edge.coverages[position]);
            }

            // Seqan consensus (run-length sequence).
            s << "<tr><td colspan=\"3\" align=\"left\"><b>Consensus base, repeat count</b></td>";
            s << "<td><b>";
            for(const Consensus& c: consensus) {
                s << c.base;
            }
            s << "</b></td>";
            s << "<td><b>";
            for(const Consensus& c: consensus) {
                if(c.base.isGap()) {
                    s << "-";
                } else {
                    const size_t repeatCount = c.repeatCount;
                    if(repeatCount < 10) {
                        s << repeatCount;
                    } else {
                        s << "*";
                    }
                }
            }
            s << "</b></td>";
            s << "</tr>";



            // Consensus coverage for each base.
            for(uint8_t b=0; b<=4; b++) {
                const AlignedBase base = AlignedBase::fromInteger(b);
                s << "<tr><td colspan=\"3\" align=\"left\"><b>Coverage for " << base;
                s << "</b></td>";
                s << "<td><b>";
                for(const Coverage& coverage: edge.coverages) {
                    s << coverage.coverageCharacter(base);
                }
                s << "</b></td>";
                s << "</tr>";
            }

            // Consensus coverage for the best base.
            s << "<tr><td colspan=\"3\" align=\"left\"><b>Coverage for consensus base</b></td>";
            s << "<td><b>";
            for(size_t position=0; position<consensus.size(); position++) {
                const AlignedBase base = consensus[position].base;
                s << edge.coverages[position].coverageCharacter(base);
            }
            s << "</b></td>";
            s << "</tr>";



            // Find the repeat counts that have non-zero coverage on the best base
            // at any position.
            const std::set<size_t> repeatCounts = graph.consensusCaller.findRepeatCounts(edge.coverages);

            // Coverage for the consensus base at each position, broken down
            // by repeat count.
            for(size_t repeatCount: repeatCounts) {
                s << "<tr><td colspan=\"4\" align=\"left\"><b>Coverage for consensus base, repeat count ";
                s << repeatCount << "</b></td>";
                s << "<td><b>";
                for(size_t position=0; position<consensus.size(); position++) {
                    const AlignedBase base = consensus[position].base;
                    const Coverage& coverage = edge.coverages[position];
                    if(base.isGap()) {
                        s << "-";
                        continue;
                    }
                    s << coverage.coverageCharacter(base, repeatCount);
                }
                s << "</b></td>";
                s << "</tr>";
            }
            s << "<tr><td colspan=\"4\" align=\"left\"><b>Coverage for consensus base and repeat count</b></td>";
            s << "<td><b>";
            for(size_t position=0; position<consensus.size(); position++) {
                const AlignedBase base = consensus[position].base;
                const size_t repeatCount = consensus[position].repeatCount;
                if(base.isGap()) {
                    s << "-";
                    continue;
                }
                const Coverage& coverage = edge.coverages[position];
                s << coverage.coverageCharacter(base, repeatCount);
            }
            s << "</b></td>";
            s << "</tr>";



            // Seqan consensus (raw sequence) and its coverage.
            s << "<tr><td colspan=\"3\" align=\"left\"><b>Consensus (raw)</b></td>";
            s << "<td colspan=\"2\"><b>";
            for(size_t position=0; position<consensus.size(); position++) {
                const AlignedBase base = consensus[position].base;
                if(!base.isGap()) {
                    const size_t repeatCount = consensus[position].repeatCount;
                    for(size_t k=0; k<repeatCount; k++) {
                        s << base;
                    }
                }
            }
            s << "</b></td>";
            s << "</tr>";
            s << "<tr><td colspan=\"3\" align=\"left\"><b>Consensus (raw) coverage</b></td>";
            s << "<td colspan=\"2\"><b>";
            for(size_t position=0; position<consensus.size(); position++) {
                const AlignedBase base = consensus[position].base;
                if(!base.isGap()) {
                    const size_t repeatCount = consensus[position].repeatCount;
                    const char coverageCharacter =
                        edge.coverages[position].coverageCharacter(base, repeatCount);
                    for(size_t k=0; k<repeatCount; k++) {
                        s << coverageCharacter;
                    }
                }
            }
            s << "</b></td>";
            s << "</tr>";
        }


        // End the label.
        s << "</table></font>> decorate=true";


        // End edge attributes.
        s << "]";
    }

}
#endif
